#include "engine.hpp"
#include "event.hpp"
#include "plateform.hpp"
#include "scheduler.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>

auto print = [](auto const& map) {
        std::cout << "Begin ===================\n";
        for (const auto& pair : map) {
                std::cout << '{' << pair.first << ": " << pair.second << "}\n";
        }
        std::cout << "End =====================\n";
};

engine::engine(const size_t nb_processors) : current_plateform(plateform(nb_processors)) {}

void engine::set_scheduler(std::shared_ptr<scheduler>& new_sched) {
        sched = new_sched;
        sched->set_engine(shared_from_this());
}

void engine::add_event(const event& new_event, const double timestamp) {
        // Define if the event need to be unique at the current timestamp
        switch (new_event.type) {
        // List of event who need to be unique
        case types::RESCHED: {
                auto it = future_list.find(timestamp);

                // If the event doesn't already exist, insert it
                if (it == future_list.end()) {
                        future_list.insert({timestamp, std::move(new_event)});
                }
        } break;

        // For other event, just insert it
        default: {
                future_list.insert({timestamp, std::move(new_event)});
        }
        }
}

void engine::simulation() {
        while (!future_list.empty()) {
                // Take the next event and remove it
                const auto& itr = future_list.begin();
                if (this->current_timestamp != itr->first) {
                        this->current_timestamp = itr->first;
                        std::cout << "Current time : " << current_timestamp << '\n';
                }

                const event evt = std::move(itr->second);
                future_list.erase(itr);

                std::cout << "[engine] handle " << evt << '\n';
                handle(evt);
        }
}

void engine::handle(const event& evt) {
        using enum types;

        switch (evt.type) {
        case JOB_ARRIVAL: sched->handle_job_arrival(evt); break;
        case JOB_FINISHED: sched->handle_job_finished(evt); break;
        case PLAT_SPEED_CHANGED: sched->handle_plat_speed_changed(evt); break;
        case PROC_ACTIVATED: sched->handle_proc_activated(evt); break;
        case PROC_IDLED: sched->handle_proc_idle(evt); break;
        case PROC_STOPPED: sched->handle_proc_stopped(evt); break;
        case PROC_STOPPING: sched->handle_proc_stopping(evt); break;
        case RESCHED: sched->handle_resched(evt); break;
        case SERV_ACT_CONT: sched->handle_serv_active_cont(evt); break;
        case SERV_ACT_NON_CONT: sched->handle_serv_active_non_cont(evt); break;
        case SERV_BUDGET_EXHAUSTED: sched->handle_serv_budget_exhausted(evt); break;
        case SERV_BUDGET_REPLENISHED: sched->handle_serv_budget_replenished(evt); break;
        case SERV_IDLE: sched->handle_serv_idle(evt); break;
        case SERV_RUNNING: sched->handle_serv_running(evt); break;
        case SIM_FINISHED: sched->handle_sim_finished(evt); break;
        case TASK_BLOCKED: sched->handle_task_blocked(evt); break;
        case TASK_KILLED: sched->handle_task_killed(evt); break;
        case TASK_MIGRATED: sched->handle_task_migrated(evt); break;
        case TASK_PREEMPTED: sched->handle_task_preempted(evt); break;
        case TASK_SCHEDULED: sched->handle_task_scheduled(evt); break;
        case TASK_UNBLOCKED: sched->handle_task_unblocked(evt); break;
        case TT_MODE: sched->handle_tt_mode(evt); break;
        default: sched->handle_undefined_event(evt);
        }
}
