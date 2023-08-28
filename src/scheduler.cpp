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
auto is_running_server = [](const std::shared_ptr<server>& current_server) -> bool {
        return current_server->current_state == server::state::running;
};

auto is_active_server = [](const std::shared_ptr<server>& current_server) -> bool {
        return current_server->current_state == server::state::ready ||
               is_running_server(current_server);
};

/// Compare two servers and return true if the first have an highest priority
auto deadline_order = [](const std::shared_ptr<server>& first,
                         const std::shared_ptr<server>& second) {
        if (first->relative_deadline == second->relative_deadline) {
                if (first->current_state == server::state::running) {
                        return true;
                } else if (second->current_state == server::state::running) {
                        return false;
                } else {
                        return first->id() < second->id();
                }
        }
        return first->relative_deadline < second->relative_deadline;
};

auto get_priority = [](const types& type) -> int {
        using enum types;

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
        default: return 100;
        }
};
} // namespace

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

auto scheduler::make_server(const std::shared_ptr<task>& new_task) -> std::shared_ptr<server> {
        // If not, a server is attached to the task if not already, and the server go in
        // active state. The task remaining execution time is set with the WCET of the job.
        const auto has_task_a_server = [new_task](const std::shared_ptr<server>& serv) {
                assert(!serv->attached_task.expired());
                return serv->attached_task.lock() == new_task;
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

void scheduler::handle(std::vector<event> evts, const double& deltatime) {
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
        std::cout << "serv = " << serv->id() << "\n";

        // If a job arrived during this turn, do not change state to inactive
        if (serv->cant_be_inactive) {
                return;
        }

        serv->change_state(server::state::inactive);

        // Update running server if there is one
        auto running_server = std::ranges::find_if(servers, is_running_server);
        if (running_server != servers.end()) {
                running_server->get()->update_times();
        }

        need_resched = true;
}

void scheduler::handle_job_arrival(const std::shared_ptr<task>& new_task, const double& job_wcet) {
        using enum server::state;

        if (new_task->has_remaining_time()) {
                std::cerr << "Job arrived but the task is already looping, reject the task\n";
                return;
        }

        auto new_server = make_server(new_task);

        std::cout << "serv = " << new_server->id() << '\n';

        // Set the task remaining execution time with the WCET of the job
        new_task->remaining_execution_time = job_wcet;

        if (new_server->current_state == inactive) {
                // Update the current running server
                auto running_server = std::ranges::find_if(servers, is_running_server);
                if (running_server != servers.end()) {
                        (*running_server)->update_times();
                }
                new_server->virtual_time = sim()->current_timestamp;
        }

        if (new_server->current_state != ready && new_server->current_state != running) {
                new_server->change_state(ready);
                sim()->current_plateform->processors.at(0)->enqueue(new_server->attached_task);
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
        serv->update_times();

        // Looking for another job arrival of the task at the same time
        if (is_there_new_job) {
                std::cout << "A job is already plan for now\n";
                serv->postpone();
        } else {
                /// TODO move this to servers, it's not sched shit
                sim()->current_plateform->processors.at(0)->dequeue(serv->attached_task);

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
        serv->update_times();

        // Check if the job as been completed at the same time
        if (serv->attached_task.lock()->remaining_execution_time > 0) {
                serv->postpone(); // If no, postpone the deadline
        } else {
                add_trace(JOB_FINISHED, serv->id()); // If yes, trace it
        }

        this->need_resched = true;
}

void scheduler::resched() {
        using enum types;
        using enum server::state;

        add_trace(RESCHED, 0);

        // Check if there is no active and running server
        auto active_servers = servers | std::views::filter(is_active_server);

        if (std::distance(active_servers.begin(), active_servers.end()) == 0) {
                return;
        }

        auto highest_priority_server = std::ranges::min(active_servers, deadline_order);
        auto running_server = std::ranges::find_if(active_servers, is_running_server);

        // Remove all future event of type BUDGET_EXHAUSTED and JOB_FINISHED
        for (auto itr = sim()->future_list.begin(); itr != sim()->future_list.end(); ++itr) {
                const types& t = (*itr).second.type;
                if (t == SERV_BUDGET_EXHAUSTED || t == JOB_FINISHED) {
                        sim()->future_list.erase(itr);
                }
        }

        // Check that a server is running
        if (running_server != active_servers.end()) {
                // If the highest priority server has higher priority than the running server,
                // preempt the task associated with the running server
                if (highest_priority_server != (*running_server)) {
                        (*running_server)->change_state(ready);
                }
        }

        // Compute the budget of the selected server
        double new_server_budget = highest_priority_server->get_budget();
        double task_remaining_time = highest_priority_server->remaining_exec_time();

        std::cout << "budget : " << new_server_budget << "\n";
        std::cout << "remaining time : " << task_remaining_time << "\n";

        if (highest_priority_server->current_state != running) {
                highest_priority_server->change_state(running);
        }

        add_trace(SERV_BUDGET_REPLENISHED, highest_priority_server->id(), new_server_budget);

        // Insert the next event about this server in the future list
        if (new_server_budget < task_remaining_time) {
                // Insert a budget exhausted event
                sim()->add_event(
                    {SERV_BUDGET_EXHAUSTED, highest_priority_server, new_server_budget},
                    sim()->current_timestamp + new_server_budget);
        } else {
                // Insert an end of task event
                sim()->add_event({JOB_FINISHED, highest_priority_server, task_remaining_time},
                                 sim()->current_timestamp + task_remaining_time);
        }
}
