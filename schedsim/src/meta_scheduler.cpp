#include "meta_scheduler.hpp"
#include "event.hpp"
#include "protocols/traces.hpp"
#include "scheduler.hpp"
#include "schedulers/csf.hpp"
#include "schedulers/ffa.hpp"
#include "schedulers/parallel.hpp"
#include "server.hpp"
#include <algorithm>
#include <cstddef>
#include <map>
#include <platform.hpp>
#include <set>
#include <stdexcept>
#include <variant>
#include <vector>

void meta_scheduler::add_child_sched(const std::weak_ptr<cluster>& clu)
{
        schedulers.push_back(std::make_shared<csf>(sim()));
        clu.lock()->set_sched(schedulers.back()->weak_from_this());
        schedulers.back()->set_cluster(clu.lock());
}

auto compare_events(const events::event& ev1, const events::event& ev2) -> bool
{
        constexpr static int MIN_PRIORITY = 100;
        constexpr auto get_priority = overloaded{
            [](const events::job_finished&) { return 0; },
            [](const events::serv_budget_exhausted&) { return 1; },
            [](const events::serv_inactive&) { return 2; },
            [](const events::job_arrival&) { return 3; },
            [](const auto&) { return MIN_PRIORITY; }};

        return std::visit(get_priority, ev1) < std::visit(get_priority, ev2);
}

void meta_scheduler::handle(std::vector<events::event> evts)
{
        using namespace events;

        std::sort(std::begin(evts), std::end(evts), compare_events);

        // Looking for JOB_ARRIVAL events at the same time for this server
        auto has_matching_job_arrival = [&evts](const auto& server) {
                return std::any_of(evts.begin(), evts.end(), [server](const auto& evt) {
                        if (const auto* job_evt = std::get_if<events::job_arrival>(&evt)) {
                                return job_evt->task_of_job == server->get_task();
                        }
                        return false;
                });
        };

        // Process all job_finished events
        for (auto& evt : evts) {
                if (std::holds_alternative<job_finished>(evt)) {
                        auto& finished_evt = std::get<job_finished>(evt);
                        const auto server = finished_evt.server_of_job;
                        finished_evt.is_there_new_job = has_matching_job_arrival(server);
                }
        }

        // Reset all the calls to resched
        rescheds.clear();

        for (const auto& evt : evts) {
                bool i_found_a_owner = false;
                for (const auto& scheduler : schedulers) {
                        if (scheduler->is_this_my_event(evt)) {
                                scheduler->handle(evt);
                                i_found_a_owner = true;
                                break;
                        }
                }

                if ((!i_found_a_owner) && std::holds_alternative<job_arrival>(evt)) {
                        const auto& [task, _] = std::get<job_arrival>(evt);
                        const auto& [index, valid] = where_to_put_the_task(task);
                        if (valid) {
                                sim()->add_trace(protocols::traces::task_placed{
                                    task->id, index->get_cluster()->get_id()});
                                index->handle(evt);
                        }
                        else {
                                throw std::runtime_error("i duno why, but can't place this task.");
                        }
                }
        }

        for (const auto& resch : rescheds) {
                resch->call_resched();
        }
        rescheds.clear();
}
