#include "scheduler.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "job.hpp"
#include "processor.hpp"
#include "server.hpp"
#include "task.hpp"
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
} // namespace

void scheduler::set_engine(std::weak_ptr<engine> sim) {
        assert(!sim.expired());
        simulator = sim;
}

void scheduler::add_trace(const types type, const int target_id, const double payload) const {
        sim()->logging_system.add_trace({sim()->current_timestamp, type, target_id, payload});
};

auto scheduler::is_event_present(const std::shared_ptr<task>& the_task, const types type) -> bool {
        for (auto evt : sim()->future_list) {
                if (evt.first == sim()->current_timestamp && evt.second.type == type &&
                    evt.second.target.lock() == the_task) {
                        return true;
                }
        }
        return false;
}

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

void scheduler::update_server_time(const std::shared_ptr<server>& serv,
                                   const double time_consumed) {
        serv->virtual_time += time_consumed * (this->get_active_bandwidth() / serv->utilization());
        add_trace(types::VIRTUAL_TIME_UPDATE, serv->id(), serv->virtual_time);
        serv->attached_task.lock()->remaining_execution_time -= time_consumed;
};

void scheduler::postpone(const std::shared_ptr<server>& serv) {
        serv->relative_deadline += serv->period();
        add_trace(types::SERV_POSTPONE, serv->id());
}

void scheduler::goto_active_cont(const std::shared_ptr<server>& serv) {
        add_trace(types::SERV_ACT_CONT, serv->id());

        // Set the arrival time
        serv->current_state = server::state::active;
        serv->relative_deadline = sim()->current_timestamp + serv->period();

        // Insert the attached task to the first processor runqueue
        std::shared_ptr<processor> first_proc = sim()->current_plateform.processors.at(0);
        assert(!serv->attached_task.expired());
        first_proc->runqueue.push_back(serv->attached_task.lock());

        // A rescheduling is required
        sim()->add_event({types::RESCHED, serv, 0}, sim()->current_timestamp);
}

void scheduler::goto_active_non_cont(const std::shared_ptr<server>& serv) {
        // Set the state of the calling server to ACTIVE_NON_CONT
        serv->current_state = server::state::active_non_contending;
        add_trace(types::SERV_ACT_NON_CONT, serv->id());

        // Dequeue the attached task
        std::shared_ptr<processor> first_proc = sim()->current_plateform.processors.at(0);
        assert(!serv->attached_task.expired());
        for (auto itr = first_proc->runqueue.begin(); itr != first_proc->runqueue.end(); ++itr) {
                if ((*itr).lock()->id == serv->id()) {
                        first_proc->runqueue.erase(itr);
                        break;
                }
        }

        // Insert a event to pass in IDLE state
        sim()->add_event({types::SERV_IDLE, serv, 0}, serv->virtual_time);

        // A rescheduling is required
        sim()->add_event({types::RESCHED, serv, 0}, sim()->current_timestamp);
}

void scheduler::goto_idle(const std::shared_ptr<server>& serv) {
        add_trace(types::SERV_IDLE, serv->id());

        // Update the current running server because the active bandwidth has changed
        // Need to search all future event of the running task and delete it before resched
        auto running_server = std::ranges::find_if(servers, is_running_server);
        assert(running_server != servers.end());

        for (auto itr = sim()->future_list.begin(); itr != sim()->future_list.end(); ++itr) {
                const auto& evt_type = (*itr).second.type;
                if (evt_type == types::SERV_BUDGET_EXHAUSTED || evt_type == types::JOB_FINISHED) {
                        // Sure that the target is a server object
                        auto serv = std::static_pointer_cast<server>((*itr).second.target.lock());
                        if (serv->id() == (*running_server)->id()) {
                                // If the event is for the running server delete it
                                std::cout << (*itr).second << " has been deleted\n";
                                sim()->future_list.erase(itr);
                                break;
                        }
                }
        }

        sim()->add_event({types::RESCHED, serv, 0}, sim()->current_timestamp);
}

void scheduler::handle_undefined_event(const event& evt [[maybe_unused]]) {
        std::cerr << "This event is not implemented or ignored";
}

void scheduler::handle_sim_finished(const event& evt [[maybe_unused]]) {
        std::cout << "Simulation as finished\n";
        add_trace(types::SIM_FINISHED, 0);
}
void scheduler::handle_proc_activated(const event& evt [[maybe_unused]]) {}
void scheduler::handle_proc_idle(const event& evt [[maybe_unused]]) {}
void scheduler::handle_serv_budget_replenished(const event& evt [[maybe_unused]]) {}
void scheduler::handle_serv_idle(const event& evt [[maybe_unused]]) {}
void scheduler::handle_serv_running(const event& evt [[maybe_unused]]) {}
void scheduler::handle_task_preempted(const event& evt [[maybe_unused]]) {}
void scheduler::handle_task_scheduled(const event& evt [[maybe_unused]]) {}
void scheduler::handle_serv_active_cont(const event& evt [[maybe_unused]]) {}
void scheduler::handle_serv_active_non_cont(const event& evt [[maybe_unused]]) {}

void scheduler::handle_job_arrival(const event& evt) {
        // The entity is a new task
        // The payload is the wcet of the task loop
        assert(!evt.target.expired());
        std::shared_ptr<task> new_task = std::static_pointer_cast<task>(evt.target.lock());

        // Reorder event if a job finish
        if (is_event_present(new_task, types::JOB_FINISHED)) {
                sim()->add_event(evt, sim()->current_timestamp);
                std::cout << "A job finished event is in list, postpone the job arrival\n";
                return;
        }

        if (new_task->has_remaining_time()) {
                std::cerr << "Job arrived but the task is already looping, reject the task\n";
                return;
        }

        add_trace(types::JOB_ARRIVAL, new_task->id);
        // Set the task remaining execution time with the WCET of the job
        new_task->remaining_execution_time = evt.payload;

        // If not, a server is attached to the task if not already, and the server go in
        // active state. The task remaining execution time is set with the WCET of the job.
        const auto has_task_a_server = [new_task](const std::shared_ptr<server>& serv) {
                assert(!serv->attached_task.expired());
                return serv->attached_task.lock() == new_task;
        };

        // Detect if the task has a server attached, or a new server has to be created
        std::shared_ptr<server> new_server;
        auto itr = std::find_if(servers.begin(), servers.end(), has_task_a_server);
        if (itr != servers.end()) {
                // Check that the server is not already in one of active states
                assert((*itr)->current_state != server::state::active);
                new_server = (*itr);
        } else {
                // There is no server affected to the calling task, need to create one
                new_server = std::make_shared<server>(new_task);
                servers.push_back(new_server);
        }

        // If the task was already running, stay in the same state
        if (new_server->current_state == server::state::running) {
                postpone(new_server);
                // A rescheduling is require
                sim()->add_event({types::RESCHED, new_server, 0}, sim()->current_timestamp);
        } else {
                goto_active_cont(new_server);
        }

        std::cout << "virtual time = " << new_server->virtual_time << "\n";
        std::cout << "deadline = " << new_server->relative_deadline << "\n";
}

void scheduler::handle_job_finished(const event& evt) {
        assert(!evt.target.expired());
        auto serv = std::static_pointer_cast<server>(evt.target.lock());
        auto time_consumed = evt.payload; // The budget of the server when it started

        add_trace(types::JOB_FINISHED, serv->id());
        // assert(serv->current_state != server::state::active_non_contending);
        assert(serv->current_state != server::state::idle);

        // Update virtual time andd remaining execution time
        update_server_time(serv, time_consumed);

        // Looking for another job arrival of the task at the same time
        if (is_event_present(serv->attached_task.lock(), types::JOB_ARRIVAL)) {
                std::cout << "A job is already plan for now\n";
        } else {
                std::cout << "No job plan for now\n";

                if ((serv->virtual_time - sim()->current_timestamp) > 0) {
                        goto_active_non_cont(serv);
                } else {
                        goto_idle(serv);
                }
        }
        std::cout << "virtual time = " << serv->virtual_time << "\n";
        std::cout << "deadline = " << serv->relative_deadline << "\n";
}

void scheduler::handle_serv_budget_exhausted(const event& evt) {
        assert(!evt.target.expired());
        auto serv = std::static_pointer_cast<server>(evt.target.lock());
        auto time_consumed = evt.payload; // The budget of the server when it started

        add_trace(types::SERV_BUDGET_EXHAUSTED, serv->id());

        std::cout << "deadline = " << serv->relative_deadline << "\n";

        update_server_time(serv, time_consumed);

        // Check if the job as been completed at the same time
        if (serv->attached_task.lock()->remaining_execution_time > 0) {
                // If no, postpone the deadline
                serv->relative_deadline += serv->period();
                add_trace(types::SERV_POSTPONE, serv->id());
        } else {
                // If yes, trace it
                add_trace(types::JOB_FINISHED, serv->id());
        }

        sim()->add_event({types::RESCHED, serv, 0}, sim()->current_timestamp);
}

void scheduler::handle_resched(const event& evt [[maybe_unused]]) {
        // Check if there is no active and running server
        auto active_servers = servers | std::views::filter(is_active_server);
        auto highest_priority_server = std::ranges::min(active_servers, deadline_order);
        auto running_server = std::ranges::find_if(active_servers, is_running_server);

        add_trace(types::RESCHED, 0);

        // Check that a server is running
        if (running_server != active_servers.end()) {
                // If the highest priority server is higher priority than the running server,
                // preempt the task associated with the running server
                if (highest_priority_server != (*running_server)) {
                        (*running_server)->current_state = server::state::active;
                        add_trace(types::TASK_PREEMPTED, (*running_server)->id());
                }
        } else {
                // Notify if the processor start to execute a task
                const auto& proc = sim()->current_plateform.processors.at(0);
                if (proc->current_state != processor::state::running) {
                        add_trace(types::PROC_ACTIVATED, proc->id);
                }
                std::cout << "same task running\n";
        }

        // if (highest_priority_server->current_state != server::state::running) {
        highest_priority_server->current_state = server::state::running;
        add_trace(types::SERV_RUNNING, highest_priority_server->id());
        //}

        add_trace(types::TASK_SCHEDULED, highest_priority_server->id());

        // Compute the budget of the selected server
        double new_server_budget = compute_budget(*highest_priority_server);
        double task_remaining_time =
            highest_priority_server->attached_task.lock()->remaining_execution_time;

        std::cout << "budget : " << new_server_budget << "\n";
        std::cout << "remaining time : " << task_remaining_time << "\n";

        add_trace(types::SERV_BUDGET_REPLENISHED, highest_priority_server->id(), new_server_budget);

        // Insert the next event about this server in the future list
        if (new_server_budget < task_remaining_time) {
                // Insert a budget exhausted event
                sim()->add_event(
                    {types::SERV_BUDGET_EXHAUSTED, highest_priority_server, new_server_budget},
                    sim()->current_timestamp + new_server_budget);
        } else {
                // Insert an end of task event
                sim()->add_event(
                    {types::JOB_FINISHED, highest_priority_server, task_remaining_time},
                    sim()->current_timestamp + task_remaining_time);
        }
}
