#include <protocols/traces.hpp>
#include <simulator/allocator.hpp>
#include <simulator/engine.hpp>
#include <simulator/event.hpp>
#include <simulator/platform.hpp>
#include <simulator/scheduler.hpp>
#include <simulator/schedulers/csf.hpp>
#include <simulator/schedulers/ffa.hpp>
#include <simulator/schedulers/parallel.hpp>
#include <simulator/server.hpp>
#include <simulator/task.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <set>
#include <variant>
#include <vector>

namespace {
auto compare_events(const events::Event& ev1, const events::Event& ev2) -> bool
{
        constexpr static int MIN_PRIORITY = 100;
        constexpr auto get_priority = overloaded{
            [](const events::JobFinished&) { return 0; },
            [](const events::ServBudgetExhausted&) { return 1; },
            [](const events::ServInactive&) { return 2; },
            [](const events::JobArrival&) { return 3; },
            [](const auto&) { return MIN_PRIORITY; }};

        return std::visit(get_priority, ev1) < std::visit(get_priority, ev2);
}
} // namespace

namespace allocators {

auto Allocator::add_child_sched(
    const std::weak_ptr<Cluster>& clu, const std::shared_ptr<scheds::Scheduler>& sched) -> void
{
        schedulers_.push_back(std::move(sched));
        clu.lock()->scheduler(schedulers_.back()->weak_from_this());
        schedulers_.back()->cluster(clu.lock());
        call_resched(sched);
}

auto Allocator::migrate_task(
    const events::JobArrival& evt, const std::shared_ptr<scheds::Scheduler>& receiver) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto serv = evt.task_of_job->server();
        if (serv->state() == Server::State::Ready || serv->state() == Server::State::Running) {
                serv->change_state(Server::State::NonCont);
        }
        serv->been_migrated = true;
        evt.task_of_job->clear_server();

        receiver->on_job_arrival(evt.task_of_job, evt.job_duration);
}

auto Allocator::handle(std::vector<events::Event> evts) -> void
{
        using namespace events;
        using namespace protocols;

#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        std::ranges::sort(evts, compare_events);

        // Looking for JOB_ARRIVAL events at the same time for this server
        auto has_matching_job_arrival = [&evts](const auto& server) {
                return std::ranges::any_of(evts, [server](const auto& evt) {
                        if (const auto* job_evt = std::get_if<events::JobArrival>(&evt)) {
                                return job_evt->task_of_job == server->task();
                        }
                        return false;
                });
        };

        // Process all JobFinished events
        for (auto& evt : evts) {
                if (auto* finished_evt = std::get_if<JobFinished>(&evt)) {
                        finished_evt->is_there_new_job =
                            has_matching_job_arrival(finished_evt->server_of_job);
                }
        }

        // Reset all the calls to resched
        rescheds_.clear();

        for (const auto& evt : evts) {
                bool handled = false;
                for (const auto& sched : schedulers_) {
                        if (sched->is_this_my_event(evt)) {
                                sched->handle(evt);
                                handled = true;
                                break;
                        }
                }

                if (handled) { continue; }

                const auto new_job = *(std::get_if<JobArrival>(&evt));

                sim()->add_trace(traces::JobArrival{
                    .task_id = new_job.task_of_job->id(),
                    .duration = new_job.job_duration,
                    .deadline = sim()->time() + new_job.task_of_job->period()});

                const auto& receiver = where_to_put_the_task(new_job.task_of_job);

                if (receiver) {
                        // A place have been found
                        if (!new_job.task_of_job->has_server()) {
                                sim()->add_trace(traces::TaskPlaced{
                                    .task_id = new_job.task_of_job->id(),
                                    .cluster_id = receiver.value()->cluster()->id()});
                                receiver.value()->on_job_arrival(
                                    new_job.task_of_job, new_job.job_duration);
                        }
                        else if (
                            receiver.value() != new_job.task_of_job->server()->scheduler() &&
                            new_job.task_of_job->server()->state() != Server::State::Running &&
                            new_job.task_of_job->server()->state() != Server::State::Ready) {
                                sim()->add_trace(traces::TaskPlaced{
                                    .task_id = new_job.task_of_job->id(),
                                    .cluster_id = receiver.value()->cluster()->id()});
                                sim()->add_trace(traces::MigrationCluster{
                                    .task_id = new_job.task_of_job->id(),
                                    .cluster_id = receiver.value()->cluster()->id()});
                                migrate_task(new_job, receiver.value());
                        }
                        else {
                                new_job.task_of_job->server()->scheduler()->on_job_arrival(
                                    new_job.task_of_job, new_job.job_duration);
                        }
                }
                else {
                        // No place for this task
                        sim()->add_trace(traces::TaskRejected{new_job.task_of_job->id()});
                }
        }

        for (const auto& resch : rescheds_) {
                resch->call_resched();
        }
        rescheds_.clear();
}

} // namespace allocators
