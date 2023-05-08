#include "scheduler.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "job.hpp"
#include "processor.hpp"
#include "server.hpp"
#include "task.hpp"
#include "trace.hpp"
#include "tracer.hpp"

#include <algorithm>
#include <bits/ranges_algo.h>
#include <bits/ranges_base.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

namespace {
auto is_running_server = [](const std::shared_ptr<server>& current_server) -> bool {
        return current_server->current_state == server::state::running;
};

auto is_active_server = [](const std::shared_ptr<server>& current_server) -> bool {
        return current_server->current_state == server::state::active ||
               is_running_server(current_server);
};

auto deadline_order = [](const std::shared_ptr<server>& first,
                         const std::shared_ptr<server>& second) {
        return first->relative_deadline < second->relative_deadline;
};

auto compute_budget = [](const server serv) -> double { return 0; };
} // namespace

void scheduler::set_engine(std::weak_ptr<engine> sim) {
        assert(!sim.expired());
        simulator = sim;
}

void scheduler::add_trace(const trace::types& new_trace) const {
        assert(!simulator.expired());
        auto sim = simulator.lock();
        sim->logging_system.add_trace({sim->current_timestamp, new_trace});
};

auto scheduler::get_active_bandwidth() -> double {
        double active_bandwidth{0};
        for (auto serv : servers) {
                if (is_active_server(serv)) {
                        active_bandwidth += serv->utilization();
                }
        }
        return active_bandwidth;
}

auto scheduler::compute_budget(const server& serv) -> double {
        return serv.utilization() / get_active_bandwidth() *
               (serv.relative_deadline - serv.virtual_time);
}

void scheduler::handle_undefined_event(const event& evt) {
        std::cerr << "This event is not implemented or ignored";
}

void scheduler::handle_proc_activated(const event& evt) {}
void scheduler::handle_proc_idle(const event& evt) {}
void scheduler::handle_proc_stopped(const event& evt) {}
void scheduler::handle_proc_stopping(const event& evt) {}

void scheduler::handle_serv_active_cont(const event& evt) {
        assert(!simulator.expired());
        auto sim = simulator.lock();
        assert(!evt.target.expired());
        std::shared_ptr<server> serv = std::static_pointer_cast<server>(evt.target.lock());

        // Check that the server is not in active states
        assert(serv->current_state != server::state::active);
        assert(serv->current_state != server::state::running);

        // Set the arrival time
        serv->current_state = server::state::active;
        serv->relative_deadline = sim->current_timestamp + serv->period();

        // Insert the attached task to the first processor runqueue
        std::shared_ptr<processor> first_proc = sim->current_plateform.processors.at(0);
        assert(!serv->attached_task.expired());
        first_proc->runqueue.push_back(serv->attached_task.lock());

        // A rescheduling is require
        sim->add_event({types::RESCHED, serv, 0}, sim->current_timestamp);

        add_trace(trace::types::sactcont);
}

void scheduler::handle_serv_active_non_cont(const event& evt) {
        // Set the state of the calling server to ACTIVE_NON_CONT
        // Insert a event to pass in IDLE state
}

void scheduler::handle_serv_budget_exhausted(const event& evt) {
        assert(!simulator.expired());
        auto sim = simulator.lock();

        assert(!evt.target.expired());
        auto serv = std::static_pointer_cast<server>(evt.target.lock());
        auto time_consumed = evt.payload; // The budget of the server when it started

        add_trace(trace::types::sbudgetex);

        // Update time parameters
        //  - update virtual time
        serv->virtual_time += time_consumed * (this->get_active_bandwidth() / serv->utilization());
        //  - update remaining execution time
        serv->attached_task.lock()->set_remaining_execution_time(
            serv->attached_task.lock()->get_remaining_time() - time_consumed);

        // Postpone the deadline
        serv->relative_deadline += serv->period();
        add_trace(trace::types::tpostp_b);

        sim->add_event({types::RESCHED, serv, 0}, sim->current_timestamp);
}

void scheduler::handle_serv_budget_replenished(const event& evt) {}
void scheduler::handle_serv_idle(const event& evt) {}
void scheduler::handle_serv_running(const event& evt) {}
void scheduler::handle_task_blocked(const event& evt) {}
void scheduler::handle_task_killed(const event& evt) {}
void scheduler::handle_task_preempted(const event& evt) {}
void scheduler::handle_task_scheduled(const event& evt) {}
void scheduler::handle_task_unblocked(const event& evt) {}

void scheduler::handle_job_arrival(const event& evt) {
        assert(!simulator.expired());
        auto sim = simulator.lock();

        // The entity is a new task
        // The payload is the wcet of the task loop
        assert(!evt.target.expired());
        std::shared_ptr<task> new_task = std::static_pointer_cast<task>(evt.target.lock());

        // Check if the task has no execution time left
        if (new_task->has_remaining_time()) {
                std::cerr << "Job arrived but the task is already looping, reject the task\n";
                return;
        }

        // If not, a server is attached to the task if not already, and
        // the server go in active state. The task remaining execution time
        // is set with the WCET of the job.
        const auto has_task_a_server = [new_task](const std::shared_ptr<server>& serv) {
                assert(!serv->attached_task.expired());
                return serv->attached_task.lock() == new_task;
        };

        auto itr = std::find_if(servers.begin(), servers.end(), has_task_a_server);
        if (itr != servers.end()) {
                // Check that the server is not already in one of active states
                assert((*itr)->current_state != server::state::active);
                assert((*itr)->current_state != server::state::running);

                // Set the server to active state
                sim->add_event({types::SERV_ACT_CONT, (*itr), 0}, sim->current_timestamp);
        } else {
                // There is no server affected to the calling task, need to create one
                auto new_serv = std::make_shared<server>(new_task);
                servers.push_back(new_serv);

                // Set the server to active state
                sim->add_event({types::SERV_ACT_CONT, new_serv, 0}, sim->current_timestamp);
        }

        // Set the task remaining execution time with the WCET of the job
        new_task->set_remaining_execution_time(evt.payload);

        add_trace(trace::types::tarrival);
        return;
}

void scheduler::handle_job_finished(const event& evt) {}

void scheduler::handle_resched(const event& evt) {
        assert(!simulator.expired());
        auto sim = simulator.lock();

        // Check if there is no active and running server
        auto active_servers = servers | std::views::filter(is_active_server);
        auto highest_priority_server = std::ranges::min(active_servers, deadline_order);
        auto running_server = std::ranges::find_if(active_servers, is_running_server);

        // Check that a server is running
        if (running_server != active_servers.end()) {
                // There is an running server

                // If the highest priority server is higher priority than the running server,
                // preempt the task associated with the running server
                if (highest_priority_server != (*running_server)) {
                        (*running_server)->current_state = server::state::active;
                        add_trace(trace::types::tpreempted);
                }
        } else {
                // There is no running server

                // Notify if the processor start to execute a task
                if (sim->current_plateform.processors.at(0)->current_state !=
                    processor::state::running) {
                        add_trace(trace::types::pactivated);
                }
        }
        add_trace(trace::types::tbegin);
        highest_priority_server->current_state = server::state::running;

        // Compute the budget of the selected server
        double new_server_budget = compute_budget(*highest_priority_server);

        // Insert the next event about this server in the future list
        if (new_server_budget <
            highest_priority_server->attached_task.lock()->get_remaining_time()) {
                // Insert a budget exhausted event
                sim->add_event(
                    {types::SERV_BUDGET_EXHAUSTED, highest_priority_server, new_server_budget},
                    sim->current_timestamp + new_server_budget);
        } else {
                // Insert an end of task event
                sim->add_event(
                    {types::JOB_FINISHED, highest_priority_server, new_server_budget},
                    sim->current_timestamp +
                        highest_priority_server->attached_task.lock()->get_remaining_time());
        }
}
