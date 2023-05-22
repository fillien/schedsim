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

engine::engine(const size_t nb_processors)
    : current_plateform(plateform(nb_processors)), logging_system(&this->current_timestamp) {}

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
        int cpt_burst{0};
        std::cout << "\033[1;31m"
                  << "==== Time 0 ====\033[0m\n";
        while (!future_list.empty() && future_list.begin()->second.type != types::SIM_FINISHED &&
               cpt_burst < 10) {
                std::cout << cpt_burst << std::endl;

                // Take the next event and remove it
                const std::multimap<double, const event>::iterator itr = future_list.begin();
                std::cout << "-----------\n";
                if (this->current_timestamp != itr->first) {
                        cpt_burst = 0;
                        this->current_timestamp = itr->first;
                        std::cout << "\033[1;31m"
                                  << "==== Time " << current_timestamp << " ====\033[0m\n";
                }

                const event evt = std::move(itr->second);

                // If the event is a RESCHED, ensure that the last event of the timestamp
                if (evt.type == types::RESCHED) {
                        // std::cout << "Current event is a RESCHED\n";
                        auto current_key = future_list.equal_range(current_timestamp);
                        // std::cout << "Distance = "
                        //           << std::distance(current_key.first, current_key.second) <<
                        //           "\n";
                        if (std::distance(current_key.first, current_key.second) > 1) {
                                future_list.erase(itr);
                                future_list.insert({current_timestamp, std::move(evt)});
                                std::cout
                                    << "Replace RESCHED at the end of the current timestamp\n";
                                continue;
                        }
                }

                future_list.erase(itr);

                std::cout << "[engine] handle " << evt << '\n';
                handle(evt);
                cpt_burst++;
        }
        if (future_list.begin()->second.type == types::SIM_FINISHED) {
                handle(future_list.begin()->second);
        }
}

void engine::handle(const event& evt) {
        using enum types;

        switch (evt.type) {
        case JOB_ARRIVAL: sched->handle_job_arrival(evt); break;
        case JOB_FINISHED: sched->handle_job_finished(evt); break;
        case PROC_ACTIVATED: sched->handle_proc_activated(evt); break;
        case PROC_IDLED: sched->handle_proc_idle(evt); break;
        case RESCHED: sched->handle_resched(evt); break;
        case SERV_ACT_CONT: sched->handle_serv_active_cont(evt); break;
        case SERV_ACT_NON_CONT: sched->handle_serv_active_non_cont(evt); break;
        case SERV_BUDGET_EXHAUSTED: sched->handle_serv_budget_exhausted(evt); break;
        case SERV_BUDGET_REPLENISHED: sched->handle_serv_budget_replenished(evt); break;
        case SERV_IDLE: sched->handle_serv_idle(evt); break;
        case SERV_RUNNING: sched->handle_serv_running(evt); break;
        case SIM_FINISHED: sched->handle_sim_finished(evt); break;
        case TASK_PREEMPTED: sched->handle_task_preempted(evt); break;
        case TASK_SCHEDULED: sched->handle_task_scheduled(evt); break;
        default: sched->handle_undefined_event(evt);
        }
}
