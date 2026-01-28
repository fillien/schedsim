#include <schedsim/algo/edf_scheduler.hpp>

#include <algorithm>
#include <cassert>

namespace schedsim::algo {

EdfScheduler::EdfScheduler(core::Engine& engine, std::vector<core::Processor*> processors)
    : engine_(engine)
    , processors_(std::move(processors))
    , reference_performance_(engine.platform().reference_performance()) {

    // Register deferred callback for rescheduling
    resched_deferred_ = engine.register_deferred([this]() { on_resched(); });

    // Install ISR handlers on all managed processors
    for (auto* proc : processors_) {
        proc->set_job_completion_handler([this](core::Processor& p, core::Job& j) {
            on_job_completion(p, j);
        });
        proc->set_deadline_miss_handler([this](core::Processor& p, core::Job& j) {
            on_deadline_miss(p, j);
        });
        proc->set_processor_available_handler([this](core::Processor& p) {
            on_processor_available(p);
        });
    }
}

EdfScheduler::~EdfScheduler() {
    // Cancel all budget timers
    for (auto& [server, timer] : budget_timers_) {
        engine_.cancel_timer(timer);
    }
}

void EdfScheduler::on_job_arrival(core::Task& task, core::Job job) {
    CbsServer* server = find_server(task);
    if (!server) {
        // Auto-create server with task parameters if not exists
        server = &add_server(task);
    }

    bool was_inactive = (server->state() == CbsServer::State::Inactive);

    // Enqueue the job
    server->enqueue_job(std::move(job));

    // If server was inactive, activate it
    if (was_inactive) {
        server->activate(engine_.time());
    }

    // Request reschedule
    request_resched();
}

bool EdfScheduler::can_admit(core::Duration budget, core::Duration period) const {
    double new_util = budget.count() / period.count();
    double capacity = static_cast<double>(processors_.size());

    // For uniprocessor: sum(U_i) + new_U <= 1.0
    // For multiprocessor: sum(U_i) + new_U <= m (basic bound)
    return (total_utilization_ + new_util) <= capacity;
}

double EdfScheduler::utilization() const {
    return total_utilization_;
}

std::span<core::Processor* const> EdfScheduler::processors() const {
    return processors_;
}

CbsServer& EdfScheduler::add_server(core::Task& task, core::Duration budget, core::Duration period,
                                     CbsServer::OverrunPolicy policy) {
    double new_util = budget.count() / period.count();
    double capacity = static_cast<double>(processors_.size());

    if ((total_utilization_ + new_util) > capacity) {
        throw AdmissionError(new_util, capacity - total_utilization_);
    }

    return add_server_unchecked(task, budget, period, policy);
}

CbsServer& EdfScheduler::add_server(core::Task& task) {
    return add_server(task, task.wcet(), task.period());
}

CbsServer& EdfScheduler::add_server_unchecked(core::Task& task, core::Duration budget,
                                               core::Duration period,
                                               CbsServer::OverrunPolicy policy) {
    servers_.emplace_back(next_server_id_++, budget, period, policy);
    CbsServer& server = servers_.back();
    server.set_task(&task);
    server.set_scheduler(this);

    task_to_server_[&task] = &server;
    total_utilization_ += server.utilization();

    return server;
}

CbsServer* EdfScheduler::find_server(core::Task& task) {
    auto it = task_to_server_.find(&task);
    return (it != task_to_server_.end()) ? it->second : nullptr;
}

const CbsServer* EdfScheduler::find_server(const core::Task& task) const {
    auto it = task_to_server_.find(const_cast<core::Task*>(&task));
    return (it != task_to_server_.end()) ? it->second : nullptr;
}

void EdfScheduler::set_deadline_miss_policy(DeadlineMissPolicy policy) {
    deadline_miss_policy_ = policy;
}

void EdfScheduler::set_deadline_miss_handler(
    std::function<void(core::Processor&, core::Job&)> handler) {
    deadline_miss_handler_ = std::move(handler);
}

// ISR Handlers

void EdfScheduler::on_job_completion(core::Processor& proc, core::Job& job) {
    CbsServer* server = find_server(job.task());
    if (!server) {
        return;
    }

    // Cancel budget timer
    cancel_budget_timer(*server);

    // Compute execution time since dispatch
    auto it = last_dispatch_time_.find(server);
    if (it != last_dispatch_time_.end()) {
        core::Duration wall_elapsed = engine_.time() - it->second;
        core::Duration ref_executed = core::Duration{
            wall_elapsed.count() * proc.speed(reference_performance_)};
        server->update_virtual_time(ref_executed);
        last_dispatch_time_.erase(it);
    }

    // Remove job from server queue
    if (server->has_pending_jobs()) {
        server->dequeue_job();
    }

    // Transition server state
    server->complete_job(engine_.time());

    // Remove mappings
    server_to_processor_.erase(server);
    processor_to_server_.erase(&proc);

    // Request reschedule
    request_resched();
}

void EdfScheduler::on_deadline_miss(core::Processor& proc, core::Job& job) {
    // Call custom handler if set
    if (deadline_miss_handler_) {
        deadline_miss_handler_(proc, job);
    }

    // Apply policy
    switch (deadline_miss_policy_) {
        case DeadlineMissPolicy::Continue:
            // Just log and continue (logging would be done by handler)
            break;

        case DeadlineMissPolicy::AbortJob: {
            // Abort the job, clear from processor
            CbsServer* server = find_server(job.task());
            if (server) {
                cancel_budget_timer(*server);
                if (server->has_pending_jobs()) {
                    server->dequeue_job();
                }
                server->complete_job(engine_.time());
                server_to_processor_.erase(server);
                processor_to_server_.erase(&proc);
                last_dispatch_time_.erase(server);
            }
            proc.clear();
            request_resched();
            break;
        }

        case DeadlineMissPolicy::AbortTask:
            // Remove task from scheduler entirely
            // Note: This is a simplified implementation
            // In practice, we'd need to clean up all server state
            proc.clear();
            request_resched();
            break;

        case DeadlineMissPolicy::StopSimulation:
            // This would require engine support for stopping
            // For now, just abort the job
            proc.clear();
            break;
    }
}

void EdfScheduler::on_processor_available(core::Processor& /*proc*/) {
    // Request reschedule when a processor becomes available
    request_resched();
}

// Deferred callback

void EdfScheduler::on_resched() {
    dispatch_edf();
}

void EdfScheduler::request_resched() {
    engine_.request_deferred(resched_deferred_);
}

// EDF dispatch

void EdfScheduler::dispatch_edf() {
    // Get ready servers sorted by deadline (ascending = earlier deadline first)
    auto ready = get_ready_servers();
    std::sort(ready.begin(), ready.end(),
              [](const CbsServer* a, const CbsServer* b) {
                  if (a->deadline() != b->deadline()) {
                      return a->deadline() < b->deadline();
                  }
                  return a->id() < b->id();  // Deterministic tie-breaker
              });

    // Get available processors (Idle state)
    auto available = get_available_processors();

    // Check for preemption: if a ready server has earlier deadline than a running one
    for (auto* proc : processors_) {
        if (proc->state() != core::ProcessorState::Running) {
            continue;
        }

        CbsServer* running = find_server_on_processor(*proc);
        if (!running) {
            continue;
        }

        // Check if any ready server has earlier deadline
        for (auto* ready_server : ready) {
            if (ready_server->deadline() < running->deadline()) {
                // Preempt the running server
                preempt_processor(*proc);
                available.push_back(proc);
                // The preempted server is now Ready and will be in the ready list
                ready.push_back(running);
                break;
            }
        }
    }

    // Re-sort after possible additions from preemption
    std::sort(ready.begin(), ready.end(),
              [](const CbsServer* a, const CbsServer* b) {
                  if (a->deadline() != b->deadline()) {
                      return a->deadline() < b->deadline();
                  }
                  return a->id() < b->id();  // Deterministic tie-breaker
              });

    // Assign servers to processors in EDF order
    for (auto* server : ready) {
        if (available.empty()) {
            break;
        }
        if (server->state() != CbsServer::State::Ready) {
            continue;  // Skip if already dispatched
        }
        auto* proc = available.back();
        available.pop_back();
        assign_server_to_processor(*server, *proc);
    }
}

void EdfScheduler::assign_server_to_processor(CbsServer& server, core::Processor& proc) {
    assert(server.state() == CbsServer::State::Ready);
    assert(proc.state() == core::ProcessorState::Idle);
    assert(server.has_pending_jobs());

    // Get current job
    core::Job* job = server.current_job();
    assert(job != nullptr);

    // Assign job to processor
    proc.assign(*job);

    // Update server state
    server.dispatch();

    // Record mappings
    server_to_processor_[&server] = &proc;
    processor_to_server_[&proc] = &server;

    // Record dispatch time for budget consumption
    last_dispatch_time_[&server] = engine_.time();

    // Schedule budget timer
    schedule_budget_timer(server, proc);
}

void EdfScheduler::preempt_processor(core::Processor& proc) {
    CbsServer* server = find_server_on_processor(proc);
    if (!server) {
        return;
    }

    // Cancel budget timer
    cancel_budget_timer(*server);

    // Compute execution since dispatch and update budget/virtual time
    auto it = last_dispatch_time_.find(server);
    if (it != last_dispatch_time_.end()) {
        core::Duration wall_elapsed = engine_.time() - it->second;
        core::Duration ref_executed = core::Duration{
            wall_elapsed.count() * proc.speed(reference_performance_)};
        server->consume_budget(ref_executed);
        server->update_virtual_time(ref_executed);
        last_dispatch_time_.erase(it);
    }

    // Transition server to Ready
    server->preempt();

    // Clear processor (Library 1 call - updates job.remaining_work)
    proc.clear();

    // Remove mappings
    server_to_processor_.erase(server);
    processor_to_server_.erase(&proc);
}

// Budget timers

void EdfScheduler::schedule_budget_timer(CbsServer& server, core::Processor& proc) {
    // Note: Budget timer is computed at dispatch time. If DVFS changes frequency
    // during execution, this timer will be inaccurate. Phase 6 adds DVFS policies
    // with proper budget timer rescheduling.
    core::Duration remaining = server.remaining_budget();
    double speed = proc.speed(reference_performance_);

    // Wall time until budget exhaustion: remaining / speed
    core::Duration wall_time{remaining.count() / speed};
    core::TimePoint exhaust_time = engine_.time() + wall_time;

    // Only schedule if in the future
    if (exhaust_time > engine_.time()) {
        budget_timers_[&server] = engine_.add_timer(
            exhaust_time,
            core::EventPriority::TIMER_DEFAULT,
            [this, &server]() { on_budget_exhausted(server); });
    }
}

void EdfScheduler::cancel_budget_timer(CbsServer& server) {
    auto it = budget_timers_.find(&server);
    if (it != budget_timers_.end()) {
        engine_.cancel_timer(it->second);
        budget_timers_.erase(it);
    }
}

void EdfScheduler::on_budget_exhausted(CbsServer& server) {
    // Remove from timer map (timer has fired)
    budget_timers_.erase(&server);

    // Find processor running this server
    auto it = server_to_processor_.find(&server);
    if (it == server_to_processor_.end()) {
        return;  // Server not running
    }
    core::Processor* proc = it->second;

    // Update virtual time for executed portion
    auto dispatch_it = last_dispatch_time_.find(&server);
    if (dispatch_it != last_dispatch_time_.end()) {
        core::Duration wall_elapsed = engine_.time() - dispatch_it->second;
        core::Duration ref_executed = core::Duration{
            wall_elapsed.count() * proc->speed(reference_performance_)};
        server.update_virtual_time(ref_executed);
        last_dispatch_time_.erase(dispatch_it);
    }

    // Clear processor
    proc->clear();

    // Transition server state (postpones deadline)
    server.exhaust_budget(engine_.time());

    // Remove mappings
    server_to_processor_.erase(&server);
    processor_to_server_.erase(proc);

    // Request reschedule
    request_resched();
}

CbsServer* EdfScheduler::find_server_on_processor(core::Processor& proc) {
    auto it = processor_to_server_.find(&proc);
    return (it != processor_to_server_.end()) ? it->second : nullptr;
}

std::vector<CbsServer*> EdfScheduler::get_ready_servers() {
    std::vector<CbsServer*> ready;
    for (auto& server : servers_) {
        if (server.state() == CbsServer::State::Ready) {
            ready.push_back(&server);
        }
    }
    return ready;
}

std::vector<core::Processor*> EdfScheduler::get_available_processors() {
    std::vector<core::Processor*> available;
    for (auto* proc : processors_) {
        if (proc->state() == core::ProcessorState::Idle) {
            available.push_back(proc);
        }
    }
    return available;
}

} // namespace schedsim::algo
