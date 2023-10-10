#include "scheduler.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "processor.hpp"
#include "server.hpp"
#include "task.hpp"
#include "tracer.hpp"

#include <algorithm>
#include <bits/ranges_algo.h>
#include <bits/ranges_base.h>
#include <bits/ranges_util.h>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

namespace {

auto get_priority(const types& type) -> int {
        using enum types;
        constexpr int MIN_PRIORITY = 100;

        // Less is more priority
        switch (type) {
        JOB_FINISHED:
                return 0;
        SERV_BUDGET_EXHAUSTED:
                return 1;
        JOB_ARRIVAL:
                return 2;
        SERV_INACTIVE:
                return 3;
        default: return MIN_PRIORITY;
        }
};
} // namespace

auto scheduler::is_running_server(const std::shared_ptr<server>& current_server) -> bool {
        return current_server->current_state == server::state::running;
}

auto scheduler::is_ready_server(const std::shared_ptr<server>& current_server) -> bool {
        return current_server->current_state == server::state::ready;
}

auto scheduler::is_active_server(const std::shared_ptr<server>& current_server) -> bool {
        return current_server->current_state != server::state::inactive;
}

/// Compare two servers and return true if the first have an highest priority
auto scheduler::deadline_order(const std::shared_ptr<server>& first,
                               const std::shared_ptr<server>& second) -> bool {
        if (first->relative_deadline == second->relative_deadline) {
                if (first->current_state == server::state::running) {
                        return true;
                }
                if (second->current_state == server::state::running) {
                        return false;
                }
                return first->id() < second->id();
        }
        return first->relative_deadline < second->relative_deadline;
}

void scheduler::add_trace(const types type, const int target_id, const double payload) const {
        sim()->logging_system.add_trace({sim()->current_timestamp, type, target_id, payload});
}

auto scheduler::is_event_present(const std::shared_ptr<task>& the_task, const types type) -> bool {
        const auto timestamp = sim()->current_timestamp;
        return std::any_of(sim()->future_list.begin(), sim()->future_list.end(),
                           [&timestamp, &type, &the_task](auto const& evt) -> bool {
                                   return evt.first == timestamp && evt.second.type == type &&
                                          evt.second.target.lock() == the_task;
                           });
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

auto scheduler::make_server(const std::shared_ptr<task>& new_task) -> std::shared_ptr<server> {
        // If not, a server is attached to the task if not already, and the server go in
        // active state. The task remaining execution time is set with the WCET of the job.
        const auto has_task_a_server = [new_task](const std::shared_ptr<server>& serv) {
                return serv->attached_task == new_task;
        };

        // Detect if the task has a server attached, otherwise a new server
        // has to be created and attached
        std::shared_ptr<server> new_server;
        auto itr = std::find_if(servers.begin(), servers.end(), has_task_a_server);
        if (itr != servers.end()) {
                new_server = (*itr);
        } else {
                // There is no server affected to the awaking task, need to create one
                new_server = std::make_shared<server>(sim(), new_task);
                servers.push_back(new_server);
        }
        return new_server;
}

void scheduler::handle(std::vector<event> evts) {
        // Sort events according to event priority cf:get_priority function
        std::sort(evts.begin(), evts.end(), [](const event& ev1, const event& ev2) {
                return get_priority(ev1.type) > get_priority(ev2.type);
        });

        // Reset flags
        this->need_resched = false;

        for (const auto& evt : evts) {
                using enum types;
                std::cout << evt.type << std::endl;

                switch (evt.type) {
                case JOB_FINISHED: {
                        // Looking for JOB_ARRIVAL events at the same time for this server
                        auto is_there_new_job{false};
                        for (auto future_evt : evts) {
                                if (future_evt.type == JOB_ARRIVAL &&
                                    future_evt.target.lock() == evt.target.lock()) {
                                        is_there_new_job = true;
                                        break;
                                }
                        }
                        assert(!evt.target.expired());
                        auto serv = std::static_pointer_cast<server>(evt.target.lock());
                        handle_job_finished(serv, is_there_new_job);
                        break;
                }
                case SERV_BUDGET_EXHAUSTED: {
                        assert(!evt.target.expired());
                        auto serv = std::static_pointer_cast<server>(evt.target.lock());
                        handle_serv_budget_exhausted(serv);
                        break;
                }
                case JOB_ARRIVAL: {
                        // The entity is a new task
                        // The payload is the wcet of the task loop
                        assert(!evt.target.expired());
                        handle_job_arrival(std::static_pointer_cast<task>(evt.target.lock()),
                                           evt.payload);
                        break;
                }
                case SERV_INACTIVE: {
                        assert(!evt.target.expired());
                        auto serv = std::static_pointer_cast<server>(evt.target.lock());
                        handle_serv_inactive(serv);
                        break;
                }
                default: std::cerr << "Unknown event" << std::endl;
                }
        }

        if (this->need_resched) {
                resched();
        }
}

void scheduler::handle_serv_inactive(const std::shared_ptr<server>& serv) {
        // If a job arrived during this turn, do not change state to inactive
        if (serv->cant_be_inactive) {
                return;
        }

        serv->change_state(server::state::inactive);

        // Update running server if there is one
        auto running_server = std::ranges::find_if(servers, is_running_server);
        if (running_server != servers.end()) {
                update_server_times(*running_server);
        }

        need_resched = true;
}

void scheduler::handle_job_arrival(const std::shared_ptr<task>& new_task, const double& job_wcet) {
        using enum server::state;

        const auto has_task_a_server = [new_task](const std::shared_ptr<server>& serv) {
                return serv->attached_task == new_task;
        };

        auto new_server = make_server(new_task);

        // Set the task remaining execution time with the WCET of the job
        new_task->add_job(job_wcet);

        if (new_server->current_state == inactive) {
                if (!admission_test(new_task)) {
                        add_trace(types::TASK_REJECTED, new_task->id);
                        return;
                }

                // Update the current running server
                auto running_server = std::ranges::find_if(servers, is_running_server);
                if (running_server != servers.end()) {
                        update_server_times(*running_server);
                }
                new_server->virtual_time = sim()->current_timestamp;
        }

        if (new_server->current_state != ready && new_server->current_state != running) {
                new_server->change_state(ready);
                this->need_resched = true;
        }

        sim()->logging_system.traceJobArrival(new_server->id(), new_server->virtual_time,
                                              new_server->relative_deadline);
}

void scheduler::handle_job_finished(const std::shared_ptr<server>& serv, bool is_there_new_job) {
        using enum types;
        using enum server::state;

        assert(serv->current_state != inactive);
        add_trace(JOB_FINISHED, serv->id());

        // Update virtual time and remaining execution time
        update_server_times(serv);

        if (serv->attached_task->has_job()) {
                serv->attached_task->next_job();
                serv->postpone();
        } else if (is_there_new_job) {
                // Looking for another job arrival of the task at the same time
                serv->postpone();
        } else {
                serv->attached_task->attached_proc->clear_server();

                if ((serv->virtual_time - sim()->current_timestamp) > 0) {
                        serv->change_state(non_cont);
                } else {
                        serv->change_state(inactive);
                }
        }
        this->need_resched = true;

        std::cout << "virtual time = " << serv->virtual_time << "\n";
        std::cout << "deadline = " << serv->relative_deadline << "\n";
}

void scheduler::handle_serv_budget_exhausted(const std::shared_ptr<server>& serv) {
        using enum types;
        add_trace(SERV_BUDGET_EXHAUSTED, serv->id());
        update_server_times(serv);

        // Check if the job as been completed at the same time
        if (serv->attached_task->get_remaining_time() > 0) {
                serv->postpone(); // If no, postpone the deadline
        } else {
                add_trace(JOB_FINISHED, serv->id()); // If yes, trace it
        }

        this->need_resched = true;
}

void scheduler::update_server_times(const std::shared_ptr<server>& serv) {
        assert(serv->current_state == server::state::running);

        const double running_time = sim()->current_timestamp - serv->last_update;
        std::cout << "running_time = " << running_time;
        std::cout << "\nremaining_time = " << serv->attached_task->get_remaining_time()
                  << std::endl;
        std::cout << serv->attached_task->get_remaining_time() - running_time << std::endl;

        // Be careful about floating point number computation near 0
        assert((serv->attached_task->get_remaining_time() - running_time) >= -engine::ZERO_ROUNDED);

        serv->virtual_time = get_server_new_virtual_time(serv, running_time);
        sim()->logging_system.add_trace(
            {sim()->current_timestamp, types::VIRTUAL_TIME_UPDATE, serv->id(), serv->virtual_time});

        serv->attached_task->consume_time(running_time);
        serv->last_update = sim()->current_timestamp;
}

void scheduler::resched() {
        add_trace(types::RESCHED, 0);
        custom_scheduler();

        for (auto const& proc : sim()->current_plateform->processors) {
                proc->update_state();
        }
}

void scheduler::resched_proc(const std::shared_ptr<processor>& proc,
                             const std::shared_ptr<server>& server_to_execute) {
        using enum types;

        if (proc->has_server_running()) {
                // Remove all future event of type BUDGET_EXHAUSTED and JOB_FINISHED of the server
                // that will be preempted
                const auto proc_server_id = proc->get_server()->id();
                const auto count =
                    std::erase_if(sim()->future_list, [proc_server_id](const auto& event) {
                            auto const& type = event.second.type;
                            if (type == SERV_BUDGET_EXHAUSTED || type == JOB_FINISHED) {
                                    auto const& serv = std::static_pointer_cast<server>(
                                        event.second.target.lock());
                                    return serv->id() == proc_server_id;
                            }
                            return false;
                    });
                std::cout << count << " future events have been removed" << std::endl;

                sim()->logging_system.add_trace(
                    {sim()->current_timestamp, types::TASK_PREEMPTED, proc_server_id, 0});
                proc->get_server()->change_state(server::state::ready);
        }
        server_to_execute->change_state(server::state::running);
        proc->set_server(server_to_execute);

        // Compute the budget of the selected server
        double new_server_budget = get_server_budget(server_to_execute);
        double task_remaining_time = server_to_execute->remaining_exec_time();

        assert(new_server_budget >= 0);
        assert(task_remaining_time >= 0);

        add_trace(SERV_BUDGET_REPLENISHED, server_to_execute->id(), new_server_budget);

        // Insert the next event about this server in the future list
        if (new_server_budget < task_remaining_time) {
                // Insert a budget exhausted event
                sim()->add_event({SERV_BUDGET_EXHAUSTED, server_to_execute, new_server_budget},
                                 sim()->current_timestamp + new_server_budget);
        } else {
                // Insert an end of task event
                sim()->add_event({JOB_FINISHED, server_to_execute, task_remaining_time},
                                 sim()->current_timestamp + task_remaining_time);
        }
}
