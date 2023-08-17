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

auto deadline_order = [](const std::shared_ptr<server>& first,
                         const std::shared_ptr<server>& second) {
        if (first->relative_deadline == second->relative_deadline) {
		return first->id() < second->id();		
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
                        auto is_there_new_job{false};
                        for (auto future_evt : evts) {
                                if (future_evt.type == JOB_ARRIVAL &&
                                    future_evt.target.lock() == evt.target.lock()) {
                                        is_there_new_job = true;
                                        break;
                                }
                        }

                        handle_job_finished(evt, is_there_new_job);
                        break;
                }
                case SERV_BUDGET_EXHAUSTED: handle_serv_budget_exhausted(evt); break;
                case JOB_ARRIVAL: handle_job_arrival(evt); break;
                case SERV_INACTIVE: handle_serv_inactive(evt); break;
                default: std::cerr << "Unknown event" << std::endl;
                }
        }

        if (this->need_resched) {
                resched();
        }
}

void scheduler::handle_serv_inactive(const event& evt) {
        using enum types;

        assert(!evt.target.expired());
        auto serv = std::static_pointer_cast<server>(evt.target.lock());
        serv->current_state = server::state::inactive;
        add_trace(SERV_INACTIVE, serv->id());
        need_resched = true;
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
        using enum types;

        serv->virtual_time += time_consumed * (this->get_active_bandwidth() / serv->utilization());
        add_trace(VIRTUAL_TIME_UPDATE, serv->id(), serv->virtual_time);
        serv->attached_task.lock()->remaining_execution_time -= time_consumed;
};

void scheduler::postpone(const std::shared_ptr<server>& serv) {
        using enum types;

        serv->relative_deadline += serv->period();
        add_trace(SERV_POSTPONE, serv->id());
}

void scheduler::goto_non_cont(const std::shared_ptr<server>& serv) {
        using enum types;

        // Set the state of the calling server to NON_CONT
        serv->current_state = server::state::non_cont;
        add_trace(SERV_NON_CONT, serv->id());

        // Dequeue the attached task of the processor
        std::shared_ptr<processor> first_proc = sim()->current_plateform.processors.at(0);
        assert(!serv->attached_task.expired());
        for (auto itr = first_proc->runqueue.begin(); itr != first_proc->runqueue.end(); ++itr) {
                if ((*itr).lock()->id == serv->id()) {
                        first_proc->runqueue.erase(itr);
                        break;
                }
        }

        // Insert a event to pass in IDLE state
        sim()->add_event({SERV_INACTIVE, serv, 0}, serv->virtual_time);
}

void scheduler::handle_undefined_event(const event& evt [[maybe_unused]]) {
        std::cerr << "This event is not implemented or ignored";
}

void scheduler::handle_sim_finished(const event& evt [[maybe_unused]]) {
        using enum types;

        std::cout << "Simulation as finished\n";
        add_trace(SIM_FINISHED, 0);
}

void scheduler::handle_job_arrival(const event& evt) {
        // The entity is a new task
        // The payload is the wcet of the task loop
        assert(!evt.target.expired());
        std::shared_ptr<task> new_task = std::static_pointer_cast<task>(evt.target.lock());

        if (new_task->has_remaining_time()) {
                std::cerr << "Job arrived but the task is already looping, reject the task\n";
                return;
        }

        // Set the task remaining execution time with the WCET of the job
        new_task->remaining_execution_time = evt.payload;

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
                new_server = std::make_shared<server>(new_task);
                servers.push_back(new_server);
        }

        if (new_server->current_state == server::state::inactive) {
                // Update the current running server
                auto running_server = std::ranges::find_if(servers, is_running_server);
                if (running_server != servers.end()) {
                        update_server_time((*running_server),
                                           sim()->current_timestamp - last_resched);
                }

                new_server->virtual_time = sim()->current_timestamp;
        }

        if (new_server->current_state != server::state::ready &&
            new_server->current_state != server::state::running) {
		std::cout << "The server wasn't active\n";
		
                // Set the arrival time
                new_server->current_state = server::state::ready;
                new_server->relative_deadline = sim()->current_timestamp + new_server->period();

                // Insert the attached task to the first processor runqueue
                std::shared_ptr<processor> first_proc = sim()->current_plateform.processors.at(0);
                assert(!new_server->attached_task.expired());
                first_proc->runqueue.push_back(new_server->attached_task.lock());

                this->need_resched = true;
                sim()->logging_system.traceGotoReady(new_server->id());
        }

        sim()->logging_system.traceJobArrival(new_server->id(), new_server->virtual_time,
                                              new_server->relative_deadline);
}

void scheduler::handle_job_finished(const event& evt, bool is_there_new_job) {
        using enum types;

        assert(!evt.target.expired());
        auto serv = std::static_pointer_cast<server>(evt.target.lock());
        auto time_consumed = evt.payload; // The budget of the server when it started

        add_trace(JOB_FINISHED, serv->id());
        assert(serv->current_state != server::state::inactive);

        // Update virtual time and remaining execution time
        update_server_time(serv, time_consumed);

        // Looking for another job arrival of the task at the same time
        if (is_there_new_job) {
                std::cout << "A job is already plan for now\n";
                postpone(serv);
        } else {
                std::cout << "No job plan for now\n";
                if ((serv->virtual_time - sim()->current_timestamp) > 0) {
                        goto_non_cont(serv);
                } else {
                        handle_serv_inactive(evt);
                }
        }
	this->need_resched = true;

        std::cout << "virtual time = " << serv->virtual_time << "\n";
        std::cout << "deadline = " << serv->relative_deadline << "\n";
}

void scheduler::handle_serv_budget_exhausted(const event& evt) {
        using enum types;

        assert(!evt.target.expired());
        auto serv = std::static_pointer_cast<server>(evt.target.lock());
        const auto time_consumed = evt.payload; // The budget of the server when it started

        add_trace(SERV_BUDGET_EXHAUSTED, serv->id());

        update_server_time(serv, time_consumed);

        // Check if the job as been completed at the same time
        if (serv->attached_task.lock()->remaining_execution_time > 0) {
                // If no, postpone the deadline
                postpone(serv);
        } else {
                // If yes, trace it
                add_trace(JOB_FINISHED, serv->id());
        }

        this->need_resched = true;
}

void scheduler::resched() {
        using enum types;
        add_trace(RESCHED, 0);

        // Check if there is no active and running server
        auto active_servers = servers | std::views::filter(is_active_server);

        if (std::distance(active_servers.begin(), active_servers.end()) == 0) {
                sim()->current_plateform.processors.at(0)->current_state = processor::state::idle;
		add_trace(PROC_IDLED, sim()->current_plateform.processors.at(0)->id);
                last_resched = sim()->current_timestamp;
                return;
        }

        auto highest_priority_server = std::ranges::min(active_servers, deadline_order);
        auto running_server = std::ranges::find_if(active_servers, is_running_server);

        // Remove all future event of type BUDGET_EXHAUSTED and JOB_FINISHED
        for (auto itr = sim()->future_list.begin(); itr != sim()->future_list.end(); ++itr) {
                if ((*itr).second.type == SERV_BUDGET_EXHAUSTED ||
                    (*itr).second.type == JOB_FINISHED) {
                        std::cout << "Deleted a future event\n";
                        sim()->future_list.erase(itr);
                }
        }

        // Check that a server is running
        if (running_server != active_servers.end()) {
                // If the highest priority server has higher priority than the running server,
                // preempt the task associated with the running server
                if (highest_priority_server != (*running_server)) {
                        (*running_server)->current_state = server::state::ready;
                        add_trace(TASK_PREEMPTED, (*running_server)->id());
                }
        } else {
                // Notify if the processor start to execute a task
                const auto& proc = sim()->current_plateform.processors.at(0);
                if (proc->current_state != processor::state::running) {
                        add_trace(PROC_ACTIVATED, proc->id);
                }
                std::cout << "same task running\n";
        }

        // if (highest_priority_server->current_state != server::state::running) {
        highest_priority_server->current_state = server::state::running;
        add_trace(SERV_RUNNING, highest_priority_server->id());
        //}

        add_trace(TASK_SCHEDULED, highest_priority_server->id());

        // Compute the budget of the selected server
        double new_server_budget = compute_budget(*highest_priority_server);
        double task_remaining_time = highest_priority_server->remaining_exec_time();

        std::cout << "budget : " << new_server_budget << "\n";
        std::cout << "remaining time : " << task_remaining_time << "\n";

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

        last_resched = sim()->current_timestamp;
}
