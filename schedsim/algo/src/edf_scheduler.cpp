#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/algo/cash_policy.hpp>
#include <schedsim/algo/csf_policy.hpp>
#include <schedsim/algo/csf_timer_policy.hpp>
#include <schedsim/algo/dpm_policy.hpp>
#include <schedsim/algo/dvfs_policy.hpp>
#include <schedsim/algo/ffa_policy.hpp>
#include <schedsim/algo/ffa_timer_policy.hpp>
#include <schedsim/algo/grub_policy.hpp>

#include <schedsim/core/clock_domain.hpp>

#include <algorithm>
#include <cassert>
#include <unordered_set>

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

    // Track arrivals for detach logic
    arrival_counts_[&task]++;

    bool was_inactive = (server->state() == CbsServer::State::Inactive);
    bool was_non_contending = (server->state() == CbsServer::State::NonContending);

    // Enqueue the job
    server->enqueue_job(std::move(job));

    // If server was inactive, activate it
    if (was_inactive) {
        server->activate(engine_.time());
        notify_server_state_change(*server, ReclamationPolicy::ServerStateChange::Activated);

        // Emit serv_ready after activation
        engine_.trace([&](core::TraceWriter& w) {
            w.type("serv_ready");
            w.field("tid", static_cast<uint64_t>(task.id()));
            w.field("deadline", server->deadline().time_since_epoch().count());
            w.field("utilization", server->utilization());
        });
    } else if (was_non_contending && reclamation_policy_) {
        // NonContending -> Ready: cancel deadline timer, reactivate
        notify_server_state_change(*server, ReclamationPolicy::ServerStateChange::Activated);
        server->reactivate_from_non_contending();

        engine_.trace([&](core::TraceWriter& w) {
            w.type("serv_ready");
            w.field("tid", static_cast<uint64_t>(task.id()));
            w.field("deadline", server->deadline().time_since_epoch().count());
            w.field("utilization", server->utilization());
        });
    }

    // Notify policies of utilization change
    notify_utilization_changed();

    // Emit trace event for job arrival
    engine_.trace([&](core::TraceWriter& w) {
        w.type("job_arrival");
        w.field("tid", static_cast<uint64_t>(task.id()));
        w.field("job_id", static_cast<uint64_t>(server->last_enqueued_job_id()));
    });

    // Wake processors if needed (DPM)
    if (dpm_policy_) {
        dpm_policy_->on_processor_needed(*this);
    }

    // Request reschedule
    request_resched();
}

void EdfScheduler::set_expected_arrivals(const core::Task& task, std::size_t count) {
    expected_arrivals_[&task] = count;
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
    auto it = task_to_server_.find(&task);
    return (it != task_to_server_.end()) ? it->second : nullptr;
}

void EdfScheduler::set_deadline_miss_policy(DeadlineMissPolicy policy) {
    deadline_miss_policy_ = policy;
}

void EdfScheduler::set_deadline_miss_handler(
    std::function<void(core::Processor&, core::Job&)> handler) {
    deadline_miss_handler_ = std::move(handler);
}

// Policy management

void EdfScheduler::set_reclamation_policy(std::unique_ptr<ReclamationPolicy> policy) {
    reclamation_policy_ = std::move(policy);
}

void EdfScheduler::set_dvfs_policy(std::unique_ptr<DvfsPolicy> policy) {
    dvfs_policy_ = std::move(policy);
    // Register frequency change callback
    if (dvfs_policy_) {
        dvfs_policy_->set_frequency_changed_callback(
            [this](core::ClockDomain& domain) { on_dvfs_frequency_changed(domain); });
    }
}

void EdfScheduler::set_dpm_policy(std::unique_ptr<DpmPolicy> policy) {
    dpm_policy_ = std::move(policy);
}

void EdfScheduler::enable_grub() {
    set_reclamation_policy(std::make_unique<GrubPolicy>(*this));
}

void EdfScheduler::enable_cash() {
    set_reclamation_policy(std::make_unique<CashPolicy>(*this));
}

void EdfScheduler::enable_power_aware_dvfs(core::Duration cooldown) {
    set_dvfs_policy(std::make_unique<PowerAwareDvfsPolicy>(engine_, cooldown));
}

void EdfScheduler::enable_basic_dpm(int target_cstate) {
    set_dpm_policy(std::make_unique<BasicDpmPolicy>(target_cstate));
}

double EdfScheduler::active_utilization() const {
    if (reclamation_policy_) {
        return reclamation_policy_->active_utilization();
    }
    // Default: return total utilization (no reclamation tracking)
    return total_utilization_;
}

double EdfScheduler::scheduler_utilization() const {
    if (reclamation_policy_) {
        return reclamation_policy_->scheduler_utilization();
    }
    // Default: return total utilization (no reclamation tracking)
    return total_utilization_;
}

double EdfScheduler::max_scheduler_utilization() const {
    if (reclamation_policy_) {
        double max_val = reclamation_policy_->max_scheduler_utilization();
        if (max_val > 0.0) {
            return max_val;
        }
    }
    // Fall back to max across all servers
    return max_server_utilization();
}

double EdfScheduler::max_server_utilization() const {
    double max_util = 0.0;
    for (const auto& server : servers_) {
        max_util = std::max(max_util, server.utilization());
    }
    return max_util;
}

void EdfScheduler::enable_ffa(core::Duration cooldown, int sleep_cstate) {
    set_dvfs_policy(std::make_unique<FfaPolicy>(engine_, cooldown, sleep_cstate));
    set_dpm_policy(nullptr);  // FFA manages DPM internally
}

void EdfScheduler::enable_csf(core::Duration cooldown, int sleep_cstate) {
    set_dvfs_policy(std::make_unique<CsfPolicy>(engine_, cooldown, sleep_cstate));
    set_dpm_policy(nullptr);  // CSF manages DPM internally
}

void EdfScheduler::enable_ffa_timer(core::Duration cooldown, int sleep_cstate) {
    set_dvfs_policy(std::make_unique<FfaTimerPolicy>(engine_, cooldown, sleep_cstate));
    set_dpm_policy(nullptr);  // FFA timer manages DPM internally
}

void EdfScheduler::enable_csf_timer(core::Duration cooldown, int sleep_cstate) {
    set_dvfs_policy(std::make_unique<CsfTimerPolicy>(engine_, cooldown, sleep_cstate));
    set_dpm_policy(nullptr);  // CSF timer manages DPM internally
}

// ISR Handlers

void EdfScheduler::on_job_completion(core::Processor& proc, core::Job& job) {
    CbsServer* server = find_server(job.task());
    if (!server) {
        return;
    }

    // Capture job ID before any state changes
    uint64_t completed_job_id = server->last_enqueued_job_id();

    // Cancel budget timer
    cancel_budget_timer(*server);

    // Compute execution time since dispatch
    core::Duration remaining_budget = server->remaining_budget();
    auto it = last_dispatch_time_.find(server);
    if (it != last_dispatch_time_.end()) {
        core::Duration wall_elapsed = engine_.time() - it->second;
        core::Duration ref_executed = core::Duration{
            wall_elapsed.count() * proc.speed(reference_performance_)};

        // Update virtual time (use policy if available)
        // M-GRUB VT formula uses wall time; CBS uses reference time
        if (reclamation_policy_) {
            core::Duration vt_time = reclamation_policy_->needs_global_budget_recalculation()
                ? wall_elapsed : ref_executed;
            core::TimePoint new_vt = reclamation_policy_->compute_virtual_time(
                *server, server->virtual_time(), vt_time);
            server->set_virtual_time(new_vt);
        } else {
            server->update_virtual_time(ref_executed);
        }

        // Emit virtual_time_update after VT computation
        engine_.trace([&](core::TraceWriter& w) {
            w.type("virtual_time_update");
            w.field("tid", static_cast<uint64_t>(job.task().id()));
            w.field("virtual_time", server->virtual_time().time_since_epoch().count());
        });

        server->consume_budget(ref_executed);
        remaining_budget = server->remaining_budget();
        last_dispatch_time_.erase(it);
    }

    // Emit trace event for job completion
    engine_.trace([&](core::TraceWriter& w) {
        w.type("job_finished");
        w.field("tid", static_cast<uint64_t>(job.task().id()));
        w.field("job_id", static_cast<uint64_t>(completed_job_id));
    });

    // Remove job from server queue
    if (server->has_pending_jobs()) {
        server->dequeue_job();
    }

    // Check for early completion with reclamation policy
    // Only enter NonContending when no more pending jobs in the queue
    bool enter_non_contending = false;
    if (reclamation_policy_ && !server->has_pending_jobs()) {
        enter_non_contending = reclamation_policy_->on_early_completion(*server, remaining_budget);
    }

    // Transition server state
    if (enter_non_contending) {
        server->enter_non_contending(engine_.time());
        notify_server_state_change(*server, ReclamationPolicy::ServerStateChange::NonContending);
        engine_.trace([&](core::TraceWriter& w) {
            w.type("serv_non_cont");
            w.field("tid", static_cast<uint64_t>(job.task().id()));
        });
    } else {
        server->complete_job(engine_.time());
        // Only emit Completed + serv_inactive when server actually went Inactive
        if (server->state() == CbsServer::State::Inactive) {
            notify_server_state_change(*server, ReclamationPolicy::ServerStateChange::Completed);
            engine_.trace([&](core::TraceWriter& w) {
                w.type("serv_inactive");
                w.field("tid", static_cast<uint64_t>(job.task().id()));
            });
        }
        // If server went to Ready (has pending jobs), it stays in active set
    }

    // Remove mappings
    server_to_processor_.erase(server);
    processor_to_server_.erase(&proc);

    // Emit proc_idled
    engine_.trace([&](core::TraceWriter& w) {
        w.type("proc_idled");
        w.field("cpu", static_cast<uint64_t>(proc.id()));
        w.field("cluster_id", static_cast<uint64_t>(proc.clock_domain().id()));
    });

    // Notify processor idle for DPM
    if (dpm_policy_) {
        dpm_policy_->on_processor_idle(*this, proc);
    }

    // Notify utilization change
    notify_utilization_changed();

    // Request reschedule
    request_resched();
}

void EdfScheduler::on_deadline_miss(core::Processor& proc, core::Job& job) {
    // Look up server before any state changes (needed for trace and switch branches)
    CbsServer* server = find_server(job.task());

    // Emit trace event for deadline miss
    engine_.trace([&](core::TraceWriter& w) {
        w.type("deadline_miss");
        w.field("tid", static_cast<uint64_t>(job.task().id()));
        if (server) {
            w.field("job_id", static_cast<uint64_t>(server->last_enqueued_job_id()));
        }
        w.field("cpu", static_cast<uint64_t>(proc.id()));
    });

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
            if (server) {
                cancel_budget_timer(*server);
                if (server->has_pending_jobs()) {
                    server->dequeue_job();
                }
                server->complete_job(engine_.time());
                if (server->state() == CbsServer::State::Inactive) {
                    notify_server_state_change(*server,
                        ReclamationPolicy::ServerStateChange::Completed);
                    engine_.trace([&](core::TraceWriter& w) {
                        w.type("serv_inactive");
                        w.field("tid", static_cast<uint64_t>(job.task().id()));
                    });
                }
                server_to_processor_.erase(server);
                processor_to_server_.erase(&proc);
                last_dispatch_time_.erase(server);
            }
            proc.clear();
            engine_.trace([&](core::TraceWriter& w) {
                w.type("proc_idled");
                w.field("cpu", static_cast<uint64_t>(proc.id()));
                w.field("cluster_id", static_cast<uint64_t>(proc.clock_domain().id()));
            });
            request_resched();
            break;
        }

        case DeadlineMissPolicy::AbortTask: {
            // Remove task from scheduler entirely
            if (server) {
                // Cancel budget timer
                cancel_budget_timer(*server);

                // Clear ALL pending jobs
                while (server->has_pending_jobs()) {
                    server->dequeue_job();
                }

                // Update total utilization
                total_utilization_ -= server->utilization();

                // Complete the job and transition server to Inactive
                server->complete_job(engine_.time());
                if (server->state() == CbsServer::State::Inactive) {
                    notify_server_state_change(*server,
                        ReclamationPolicy::ServerStateChange::Completed);
                    engine_.trace([&](core::TraceWriter& w) {
                        w.type("serv_inactive");
                        w.field("tid", static_cast<uint64_t>(job.task().id()));
                    });
                }

                // Remove from all mappings
                // Note: Server remains in servers_ deque but is orphaned (no task mapping)
                // Future job arrivals for this task will auto-create a new server (fresh start)
                task_to_server_.erase(server->task());
                server_to_processor_.erase(server);
                processor_to_server_.erase(&proc);
                last_dispatch_time_.erase(server);
            }
            proc.clear();
            engine_.trace([&](core::TraceWriter& w) {
                w.type("proc_idled");
                w.field("cpu", static_cast<uint64_t>(proc.id()));
                w.field("cluster_id", static_cast<uint64_t>(proc.clock_domain().id()));
            });
            request_resched();
            break;
        }

        case DeadlineMissPolicy::StopSimulation:
            // This would require engine support for stopping
            // For now, just abort the job
            proc.clear();
            engine_.trace([&](core::TraceWriter& w) {
                w.type("proc_idled");
                w.field("cpu", static_cast<uint64_t>(proc.id()));
                w.field("cluster_id", static_cast<uint64_t>(proc.clock_domain().id()));
            });
            break;
    }
}

void EdfScheduler::on_processor_available(core::Processor& proc) {
    // Notify DPM policy of idle processor
    if (dpm_policy_ && proc.state() == core::ProcessorState::Idle) {
        dpm_policy_->on_processor_idle(*this, proc);
    }

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

    // M-GRUB: recalculate budget timers for all running servers after dispatch
    if (reclamation_policy_ && reclamation_policy_->needs_global_budget_recalculation()) {
        recalculate_all_budget_timers();
    }
}

void EdfScheduler::assign_server_to_processor(CbsServer& server, core::Processor& proc) {
    assert(server.state() == CbsServer::State::Ready);
    assert(proc.state() == core::ProcessorState::Idle ||
           proc.state() == core::ProcessorState::Sleep);
    assert(server.has_pending_jobs());

    // Get current job
    core::Job* job = server.current_job();
    assert(job != nullptr);

    // Assign job to processor
    proc.assign(*job);

    // Emit proc_activated
    engine_.trace([&](core::TraceWriter& w) {
        w.type("proc_activated");
        w.field("cpu", static_cast<uint64_t>(proc.id()));
        w.field("cluster_id", static_cast<uint64_t>(proc.clock_domain().id()));
    });

    // Update server state
    server.dispatch();
    notify_server_state_change(server, ReclamationPolicy::ServerStateChange::Dispatched);

    // Emit serv_running
    engine_.trace([&](core::TraceWriter& w) {
        w.type("serv_running");
        w.field("tid", static_cast<uint64_t>(server.task()->id()));
    });

    // Record mappings
    server_to_processor_[&server] = &proc;
    processor_to_server_[&proc] = &server;

    // Record dispatch time for budget consumption
    last_dispatch_time_[&server] = engine_.time();

    // Emit trace event for task scheduled
    engine_.trace([&](core::TraceWriter& w) {
        w.type("task_scheduled");
        w.field("tid", static_cast<uint64_t>(server.task()->id()));
        w.field("job_id", static_cast<uint64_t>(server.last_enqueued_job_id()));
        w.field("cpu", static_cast<uint64_t>(proc.id()));
    });

    // Notify DVFS policy of processor becoming active
    if (dvfs_policy_) {
        dvfs_policy_->on_processor_active(*this, proc);
    }

    // Schedule budget timer (skip if global recalculation will set it)
    if (!reclamation_policy_ || !reclamation_policy_->needs_global_budget_recalculation()) {
        schedule_budget_timer(server, proc);
    }
}

void EdfScheduler::preempt_processor(core::Processor& proc) {
    CbsServer* server = find_server_on_processor(proc);
    if (!server) {
        return;
    }

    // Emit trace event for preemption
    engine_.trace([&](core::TraceWriter& w) {
        w.type("task_preempted");
        w.field("tid", static_cast<uint64_t>(server->task()->id()));
        w.field("job_id", static_cast<uint64_t>(server->last_enqueued_job_id()));
        w.field("cpu", static_cast<uint64_t>(proc.id()));
    });

    // Cancel budget timer
    cancel_budget_timer(*server);

    // Compute execution since dispatch and update budget/virtual time
    auto it = last_dispatch_time_.find(server);
    if (it != last_dispatch_time_.end()) {
        core::Duration wall_elapsed = engine_.time() - it->second;
        core::Duration ref_executed = core::Duration{
            wall_elapsed.count() * proc.speed(reference_performance_)};
        server->consume_budget(ref_executed);

        // Update virtual time (use policy if available)
        // M-GRUB VT formula uses wall time; CBS uses reference time
        if (reclamation_policy_) {
            core::Duration vt_time = reclamation_policy_->needs_global_budget_recalculation()
                ? wall_elapsed : ref_executed;
            core::TimePoint new_vt = reclamation_policy_->compute_virtual_time(
                *server, server->virtual_time(), vt_time);
            server->set_virtual_time(new_vt);
        } else {
            server->update_virtual_time(ref_executed);
        }

        // Emit virtual_time_update after VT computation
        engine_.trace([&](core::TraceWriter& w) {
            w.type("virtual_time_update");
            w.field("tid", static_cast<uint64_t>(server->task()->id()));
            w.field("virtual_time", server->virtual_time().time_since_epoch().count());
        });

        last_dispatch_time_.erase(it);
    }

    // Transition server to Ready
    server->preempt();
    notify_server_state_change(*server, ReclamationPolicy::ServerStateChange::Preempted);

    // Clear processor (Library 1 call - updates job.remaining_work)
    proc.clear();

    // Emit proc_idled
    engine_.trace([&](core::TraceWriter& w) {
        w.type("proc_idled");
        w.field("cpu", static_cast<uint64_t>(proc.id()));
        w.field("cluster_id", static_cast<uint64_t>(proc.clock_domain().id()));
    });

    // Remove mappings
    server_to_processor_.erase(server);
    processor_to_server_.erase(&proc);
}

// Budget timers

void EdfScheduler::schedule_budget_timer(CbsServer& server, core::Processor& proc) {
    core::Duration remaining;
    bool budget_is_wall_time = false;
    if (reclamation_policy_) {
        remaining = reclamation_policy_->compute_server_budget(server);
        budget_is_wall_time = reclamation_policy_->needs_global_budget_recalculation();
    } else {
        remaining = server.remaining_budget();
    }

    // M-GRUB: dynamic budget is already in wall time
    // CBS: budget is in reference time, must divide by speed
    core::Duration wall_time;
    if (budget_is_wall_time) {
        wall_time = remaining;
    } else {
        double speed = proc.speed(reference_performance_);
        wall_time = core::Duration{remaining.count() / speed};
    }
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

    // Emit trace event for budget exhaustion
    engine_.trace([&](core::TraceWriter& w) {
        w.type("serv_budget_exhausted");
        w.field("tid", static_cast<uint64_t>(server.task()->id()));
    });

    // Update virtual time for executed portion
    auto dispatch_it = last_dispatch_time_.find(&server);
    if (dispatch_it != last_dispatch_time_.end()) {
        core::Duration wall_elapsed = engine_.time() - dispatch_it->second;
        core::Duration ref_executed = core::Duration{
            wall_elapsed.count() * proc->speed(reference_performance_)};

        // Update virtual time (use policy if available)
        // M-GRUB VT formula uses wall time; CBS uses reference time
        if (reclamation_policy_) {
            core::Duration vt_time = reclamation_policy_->needs_global_budget_recalculation()
                ? wall_elapsed : ref_executed;
            core::TimePoint new_vt = reclamation_policy_->compute_virtual_time(
                server, server.virtual_time(), vt_time);
            server.set_virtual_time(new_vt);
        } else {
            server.update_virtual_time(ref_executed);
        }

        // Emit virtual_time_update after VT computation
        engine_.trace([&](core::TraceWriter& w) {
            w.type("virtual_time_update");
            w.field("tid", static_cast<uint64_t>(server.task()->id()));
            w.field("virtual_time", server.virtual_time().time_since_epoch().count());
        });

        last_dispatch_time_.erase(dispatch_it);
    }

    // Check if reclamation policy grants extra budget
    core::Duration extra_budget{0.0};
    if (reclamation_policy_) {
        extra_budget = reclamation_policy_->on_budget_exhausted(server);
    }

    // Clear processor
    proc->clear();

    // Emit proc_idled
    engine_.trace([&](core::TraceWriter& w) {
        w.type("proc_idled");
        w.field("cpu", static_cast<uint64_t>(proc->id()));
        w.field("cluster_id", static_cast<uint64_t>(proc->clock_domain().id()));
    });

    if (extra_budget.count() > 0.0) {
        // CASH: extra budget granted, continue without postponing
        // Note: Server stays Ready (was Running, exhaust puts it to Ready)
        server.exhaust_budget(engine_.time());
        // Add the extra budget on top of replenished budget
        // This is a simplified model - in practice CASH might handle this differently
    } else {
        // Standard CBS: postpone deadline
        server.exhaust_budget(engine_.time());
    }

    // Emit serv_postpone after exhaust_budget
    engine_.trace([&](core::TraceWriter& w) {
        w.type("serv_postpone");
        w.field("tid", static_cast<uint64_t>(server.task()->id()));
        w.field("deadline", server.deadline().time_since_epoch().count());
    });
    // Emit serv_budget_replenished (suppress when M-GRUB; it fires during recalculate)
    if (!reclamation_policy_ || !reclamation_policy_->needs_global_budget_recalculation()) {
        engine_.trace([&](core::TraceWriter& w) {
            w.type("serv_budget_replenished");
            w.field("tid", static_cast<uint64_t>(server.task()->id()));
            w.field("budget", server.remaining_budget().count());
        });
    }

    // Remove mappings
    server_to_processor_.erase(&server);
    processor_to_server_.erase(proc);

    // Notify DPM policy of idle processor
    if (dpm_policy_) {
        dpm_policy_->on_processor_idle(*this, *proc);
    }

    // Notify utilization change
    notify_utilization_changed();

    // Request reschedule
    request_resched();
}

void EdfScheduler::flush_running_server_times() {
    // Update VTs of all running servers using CURRENT bandwidth,
    // then reset dispatch times. Does NOT recompute budgets or timers.
    // Must be called BEFORE any operation that changes bandwidth (e.g., server activation/detach).
    for (auto* proc : processors_) {
        if (proc->state() != core::ProcessorState::Running) {
            continue;
        }

        CbsServer* server = find_server_on_processor(*proc);
        if (!server) {
            continue;
        }

        auto dispatch_it = last_dispatch_time_.find(server);
        if (dispatch_it == last_dispatch_time_.end()) {
            continue;
        }

        core::Duration wall_elapsed = engine_.time() - dispatch_it->second;
        if (wall_elapsed.count() <= 0.0) {
            continue;
        }

        core::Duration ref_executed = core::Duration{
            wall_elapsed.count() * proc->speed(reference_performance_)};

        // Update virtual time via policy (uses current bandwidth, before any change)
        if (reclamation_policy_) {
            core::Duration vt_time = reclamation_policy_->needs_global_budget_recalculation()
                ? wall_elapsed : ref_executed;
            core::TimePoint new_vt = reclamation_policy_->compute_virtual_time(
                *server, server->virtual_time(), vt_time);
            server->set_virtual_time(new_vt);
        } else {
            server->update_virtual_time(ref_executed);
        }

        // Emit virtual_time_update so verify_grub.py can see the interval boundary
        double bandwidth = reclamation_policy_
            ? reclamation_policy_->compute_bandwidth() : 1.0;
        engine_.trace([&](core::TraceWriter& w) {
            w.type("virtual_time_update");
            w.field("tid", static_cast<uint64_t>(server->task()->id()));
            w.field("virtual_time", server->virtual_time().time_since_epoch().count());
            w.field("bandwidth", bandwidth);
        });

        server->consume_budget(ref_executed);
        last_dispatch_time_[server] = engine_.time();
    }
}

void EdfScheduler::recalculate_all_budget_timers() {
    for (auto* proc : processors_) {
        if (proc->state() != core::ProcessorState::Running) {
            continue;
        }

        CbsServer* server = find_server_on_processor(*proc);
        if (!server) {
            continue;
        }

        // Update VT and budget for elapsed execution
        auto dispatch_it = last_dispatch_time_.find(server);
        if (dispatch_it != last_dispatch_time_.end()) {
            core::Duration wall_elapsed = engine_.time() - dispatch_it->second;
            core::Duration ref_executed = core::Duration{
                wall_elapsed.count() * proc->speed(reference_performance_)};

            // Update virtual time via policy
            // M-GRUB VT formula uses wall time; CBS uses reference time
            if (reclamation_policy_) {
                core::Duration vt_time = reclamation_policy_->needs_global_budget_recalculation()
                    ? wall_elapsed : ref_executed;
                core::TimePoint new_vt = reclamation_policy_->compute_virtual_time(
                    *server, server->virtual_time(), vt_time);
                server->set_virtual_time(new_vt);
            } else {
                server->update_virtual_time(ref_executed);
            }

            double bandwidth = reclamation_policy_
                ? reclamation_policy_->compute_bandwidth() : 1.0;

            // Emit virtual_time_update
            engine_.trace([&](core::TraceWriter& w) {
                w.type("virtual_time_update");
                w.field("tid", static_cast<uint64_t>(server->task()->id()));
                w.field("virtual_time", server->virtual_time().time_since_epoch().count());
                w.field("bandwidth", bandwidth);
            });

            server->consume_budget(ref_executed);
            last_dispatch_time_[server] = engine_.time();
        }

        // Cancel existing budget timer
        cancel_budget_timer(*server);

        // Compute dynamic budget via policy
        core::Duration budget;
        if (reclamation_policy_) {
            budget = reclamation_policy_->compute_server_budget(*server);
        } else {
            budget = server->remaining_budget();
        }

        // Emit serv_budget_replenished with dynamic budget
        engine_.trace([&](core::TraceWriter& w) {
            w.type("serv_budget_replenished");
            w.field("tid", static_cast<uint64_t>(server->task()->id()));
            w.field("budget", budget.count());
        });

        // Schedule timer
        // M-GRUB: dynamic budget is already in wall time (legacy: time + budget)
        // CBS: budget is in reference time, must divide by speed
        if (budget.count() > 0.0) {
            core::Duration wall_time = budget;  // M-GRUB: budget IS wall time
            core::TimePoint exhaust_time = engine_.time() + wall_time;

            if (exhaust_time > engine_.time()) {
                budget_timers_[server] = engine_.add_timer(
                    exhaust_time,
                    core::EventPriority::TIMER_DEFAULT,
                    [this, server]() { on_budget_exhausted(*server); });
            }
        }
    }
}

void EdfScheduler::try_detach_server(CbsServer& server) {
    if (server.state() != CbsServer::State::Inactive) {
        return;
    }
    if (server.has_pending_jobs()) {
        return;
    }
    const core::Task* task = server.task();
    auto exp_it = expected_arrivals_.find(task);
    auto cnt_it = arrival_counts_.find(task);
    if (exp_it != expected_arrivals_.end() && cnt_it != arrival_counts_.end()
        && cnt_it->second >= exp_it->second) {
        notify_server_state_change(server, ReclamationPolicy::ServerStateChange::Detached);
    }
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
        if (proc->state() == core::ProcessorState::Idle ||
            proc->state() == core::ProcessorState::Sleep) {
            available.push_back(proc);
        }
    }
    return available;
}

// Policy notification helpers

void EdfScheduler::notify_utilization_changed() {
    if (!dvfs_policy_) {
        return;
    }

    // Notify each clock domain
    std::unordered_set<core::ClockDomain*> notified_domains;
    for (auto* proc : processors_) {
        auto* domain = &proc->clock_domain();
        if (notified_domains.find(domain) == notified_domains.end()) {
            dvfs_policy_->on_utilization_changed(*this, *domain);
            notified_domains.insert(domain);
        }
    }
}

void EdfScheduler::notify_server_state_change(CbsServer& server,
                                                ReclamationPolicy::ServerStateChange change) {
    // M-GRUB: flush VTs before bandwidth-changing state transitions
    // so running servers have VTs computed with the old bandwidth
    if (reclamation_policy_ && reclamation_policy_->needs_global_budget_recalculation()) {
        if (change == ReclamationPolicy::ServerStateChange::Activated ||
            change == ReclamationPolicy::ServerStateChange::Detached) {
            flush_running_server_times();
        }
    }

    if (reclamation_policy_) {
        reclamation_policy_->on_server_state_change(server, change);
    }

    // After Completed or DeadlineReached, check if server should be detached (M-GRUB).
    // Note: try_detach_server() may call notify_server_state_change(Detached)
    // re-entrantly. This is safe because Detached does not match this condition,
    // so there is no infinite recursion.
    if (reclamation_policy_ && reclamation_policy_->needs_global_budget_recalculation()) {
        if (change == ReclamationPolicy::ServerStateChange::Completed ||
            change == ReclamationPolicy::ServerStateChange::DeadlineReached) {
            try_detach_server(server);
        }
    }
}

void EdfScheduler::on_dvfs_frequency_changed(core::ClockDomain& domain) {
    // Reschedule budget timers for all processors in this domain
    reschedule_budget_timers_for_domain(domain);
}

void EdfScheduler::reschedule_budget_timers_for_domain(core::ClockDomain& domain) {
    // When using M-GRUB, delegate to recalculate_all_budget_timers for consistency
    if (reclamation_policy_ && reclamation_policy_->needs_global_budget_recalculation()) {
        recalculate_all_budget_timers();
        return;
    }

    for (auto* proc : processors_) {
        if (&proc->clock_domain() != &domain) {
            continue;
        }
        if (proc->state() != core::ProcessorState::Running) {
            continue;
        }

        CbsServer* server = find_server_on_processor(*proc);
        if (!server) {
            continue;
        }

        // Cancel existing budget timer
        cancel_budget_timer(*server);

        // Update consumed budget based on elapsed time
        auto it = last_dispatch_time_.find(server);
        if (it != last_dispatch_time_.end()) {
            core::Duration wall_elapsed = engine_.time() - it->second;
            core::Duration ref_consumed = core::Duration{
                wall_elapsed.count() * proc->speed(reference_performance_)};
            server->consume_budget(ref_consumed);
            last_dispatch_time_[server] = engine_.time();
        }

        // Schedule new budget timer with current (new) speed
        schedule_budget_timer(*server, *proc);
    }
}

} // namespace schedsim::algo
