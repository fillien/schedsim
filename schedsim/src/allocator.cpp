#include <algorithm>
#include <allocator.hpp>
#include <cstddef>
#include <event.hpp>
#include <map>
#include <platform.hpp>
#include <protocols/traces.hpp>
#include <scheduler.hpp>
#include <schedulers/csf.hpp>
#include <schedulers/ffa.hpp>
#include <schedulers/parallel.hpp>
#include <server.hpp>
#include <set>
#include <stdexcept>
#include <variant>
#include <vector>

namespace allocators {

void allocator::add_child_sched(const std::weak_ptr<cluster>& clu)
{
        schedulers.push_back(std::make_shared<scheds::parallel>(sim()));
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

void allocator::handle(std::vector<events::event> evts)
{
        using namespace events;

        std::sort(std::begin(evts), std::end(evts), compare_events);

        // Looking for JOB_ARRIVAL events at the same time for this server
        auto has_matching_job_arrival = [&evts](const auto& server) {
                return std::ranges::any_of(evts, [server](const auto& evt) {
                        if (const auto* job_evt = std::get_if<events::job_arrival>(&evt)) {
                                return job_evt->task_of_job == server->get_task();
                        }
                        return false;
                });
        };

        // Process all job_finished events
        for (auto& evt : evts) {
                if (auto* finished_evt = std::get_if<job_finished>(&evt)) {
                        finished_evt->is_there_new_job =
                            has_matching_job_arrival(finished_evt->server_of_job);
                }
        }

        // Reset all the calls to resched
        rescheds.clear();

        for (const auto& evt : evts) {
                bool handled = false;
                for (const auto& scheduler : schedulers) {
                        if (scheduler->is_this_my_event(evt)) {
                                scheduler->handle(evt);
                                handled = true;
                                break;
                        }
                }

                if (!handled) {
                        if (const auto* job_evt = std::get_if<job_arrival>(&evt)) {
                                const auto& receiver = where_to_put_the_task(job_evt->task_of_job);
                                if (receiver) {
                                        sim()->add_trace(protocols::traces::task_placed{
                                            job_evt->task_of_job->id,
                                            receiver.value()->get_cluster()->get_id()});
                                        receiver.value()->handle(evt);
                                }
                                else {
                                        throw std::runtime_error(
                                            "Failed to place task: " +
                                            std::to_string(job_evt->task_of_job->id));
                                }
                        }
                }
        }

        for (const auto& resch : rescheds) {
                resch->call_resched();
        }
        rescheds.clear();
}

} // namespace allocators
