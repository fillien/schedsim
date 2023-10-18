#include "scheduler.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "processor.hpp"
#include "server.hpp"
#include "task.hpp"

#include <algorithm>
#include <bits/ranges_algo.h>
#include <bits/ranges_base.h>
#include <bits/ranges_util.h>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <ranges>
#include <variant>
#include <vector>

auto get_priority(const events::event& evt) -> int { return std::visit(priorities{}, evt); };

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

auto scheduler::get_active_bandwidth() const -> double {
        double active_bandwidth{0};
        for (const auto& serv : servers) {
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

void scheduler::handle(std::vector<events::event> evts) {
        // Sort events according to event priority cf:get_priority function
        std::sort(std::begin(evts), std::end(evts),
                  [](const events::event& ev1, const events::event& ev2) {
                          return get_priority(ev1) > get_priority(ev2);
                  });

        // Reset flags
        this->need_resched = false;

        for (const auto& evt : evts) {
                if (std::holds_alternative<events::job_finished>(evt)) {
                        // Looking for JOB_ARRIVAL events at the same time for this server
                        const auto& [serv] = std::get<events::job_finished>(evt);
                        auto is_there_new_job{false};
                        for (auto future_evt : evts) {
                                if (std::holds_alternative<events::job_arrival>(future_evt)) {
                                        const auto& [future_evt_task, job_duration] =
                                            std::get<events::job_arrival>(future_evt);
                                        if (future_evt_task == serv->attached_task) {
                                                is_there_new_job = true;
                                        }
                                }
                        }
                        handle_job_finished(serv, is_there_new_job);
                } else if (std::holds_alternative<events::serv_budget_exhausted>(evt)) {
                        const auto& [serv] = std::get<events::serv_budget_exhausted>(evt);
                        handle_serv_budget_exhausted(serv);
                } else if (std::holds_alternative<events::job_arrival>(evt)) {
                        const auto& [serv, job_duration] = std::get<events::job_arrival>(evt);
                        handle_job_arrival(serv, job_duration);
                } else if (std::holds_alternative<events::serv_inactive>(evt)) {
                        const auto& [serv] = std::get<events::serv_inactive>(evt);
                        handle_serv_inactive(serv);
                } else {
                        std::cerr << "Unknowned event" << std::endl;
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

void scheduler::handle_job_arrival(const std::shared_ptr<task>& new_task,
                                   const double& job_duration) {
        using enum server::state;

        const auto has_task_a_server = [new_task](const std::shared_ptr<server>& serv) {
                return serv->attached_task == new_task;
        };

        auto new_server = make_server(new_task);

        // Set the task remaining execution time with the WCET of the job
        new_task->add_job(job_duration);

        if (new_server->current_state == inactive) {
                if (!admission_test(new_task)) {
                        sim()->add_trace(events::task_rejected{new_task});
                        return;
                }

                // Update the current running server
                auto running_server = std::ranges::find_if(servers, is_running_server);
                if (running_server != servers.end()) {
                        update_server_times(*running_server);
                }
                new_server->virtual_time = sim()->get_time();
        }

        if (new_server->current_state != ready && new_server->current_state != running) {
                new_server->change_state(ready);
                this->need_resched = true;
        }

        sim()->add_trace(events::job_arrival{new_server->attached_task, job_duration});
}

void scheduler::handle_job_finished(const std::shared_ptr<server>& serv, bool is_there_new_job) {
        using enum server::state;

        assert(serv->current_state != inactive);
        sim()->add_trace(events::job_finished{serv});

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

                if ((serv->virtual_time - sim()->get_time()) > 0) {
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
        sim()->add_trace(events::serv_budget_exhausted{serv});
        update_server_times(serv);

        // Check if the job as been completed at the same time
        if (serv->attached_task->get_remaining_time() > 0) {
                serv->postpone(); // If no, postpone the deadline
        } else {
                sim()->add_trace(events::job_finished{serv}); // If yes, trace it
        }

        this->need_resched = true;
}

void scheduler::update_server_times(const std::shared_ptr<server>& serv) {
        assert(serv->current_state == server::state::running);

        const double running_time = sim()->get_time() - serv->last_update;
        std::cout << "running_time = " << running_time;
        std::cout << "\nremaining_time = " << serv->attached_task->get_remaining_time()
                  << std::endl;
        std::cout << serv->attached_task->get_remaining_time() - running_time << std::endl;

        // Be careful about floating point number computation near 0
        assert((serv->attached_task->get_remaining_time() - running_time) >= -engine::ZERO_ROUNDED);

        serv->virtual_time = get_server_new_virtual_time(serv, running_time);
        sim()->add_trace(events::virtual_time_update{serv->attached_task, serv->virtual_time});

        serv->attached_task->consume_time(running_time);
        serv->last_update = sim()->get_time();
}

void scheduler::resched() {
        sim()->add_trace(events::resched{});
        custom_scheduler();

        for (auto const& proc : sim()->get_plateform()->processors) {
                proc->update_state();
        }
}

void scheduler::resched_proc(const std::shared_ptr<processor>& proc,
                             const std::shared_ptr<server>& server_to_execute) {
        if (proc->has_server_running()) {
                // Remove all future event of type BUDGET_EXHAUSTED and JOB_FINISHED of the server
                // that will be preempted
                const auto proc_server_id = proc->get_server()->id();
                const auto count =
                    std::erase_if(sim()->future_list, [proc_server_id](const auto& evt) {
                            if (std::holds_alternative<events::serv_budget_exhausted>(evt.second)) {
                                    const auto& [serv] =
                                        std::get<events::serv_budget_exhausted>(evt.second);
                                    return serv->id() == proc_server_id;
                            }
                            if (std::holds_alternative<events::job_finished>(evt.second)) {
                                    const auto& [serv] = std::get<events::job_finished>(evt.second);
                                    return serv->id() == proc_server_id;
                            }
                            return false;
                    });
                std::cout << count << " future events have been removed" << std::endl;

                sim()->add_trace(events::task_preempted{proc->get_server()->attached_task});
                proc->get_server()->change_state(server::state::ready);
        }
        server_to_execute->change_state(server::state::running);
        proc->set_server(server_to_execute);

        // Compute the budget of the selected server
        double new_server_budget = get_server_budget(server_to_execute);
        double task_remaining_time = server_to_execute->remaining_exec_time();

        assert(new_server_budget >= 0);
        assert(task_remaining_time >= 0);

        sim()->add_trace(events::serv_budget_replenished{server_to_execute, new_server_budget});

        // Insert the next event about this server in the future list
        if (new_server_budget < task_remaining_time) {
                // Insert a budget exhausted event
                sim()->add_event(events::serv_budget_exhausted{server_to_execute},
                                 sim()->get_time() + new_server_budget);
        } else {
                // Insert an end of task event
                sim()->add_event(events::job_finished{server_to_execute},
                                 sim()->get_time() + task_remaining_time);
        }
}
