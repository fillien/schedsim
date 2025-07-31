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
#include <print>
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

auto Allocator::start() -> void {}
auto Allocator::end() -> void {}

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
        assert(serv->state() != Server::State::Ready && serv->state() != Server::State::Running);
        serv->been_migrated = true;
        evt.task_of_job->clear_server();

        receiver->on_job_arrival(evt.task_of_job, evt.job_duration);
}

auto Allocator::need_to_place_task(const auto& new_job) -> void
{
        using namespace protocols::traces;

        const auto& receiver = where_to_put_the_task(new_job.task_of_job);

        if (!receiver.has_value()) {
                sim()->add_trace(TaskRejected{new_job.task_of_job->id()});
                return;
        }

        const auto& scheduler = receiver.value();
        const auto& task = new_job.task_of_job;

        sim()->add_trace(
            TaskPlaced{.task_id = task->id(), .cluster_id = scheduler->cluster()->id()});

        if (!task->has_server() || task->server()->scheduler() == scheduler) {
                scheduler->on_job_arrival(task, new_job.job_duration);
        }
        else {
                // The task is on another scheduler, so it needs to be migrated.
                sim()->add_trace(MigrationCluster{
                    .task_id = task->id(), .cluster_id = scheduler->cluster()->id()});
                migrate_task(new_job, scheduler);
        }
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
                const auto& task = new_job.task_of_job;

                sim()->add_trace(traces::JobArrival{
                    .task_id = task->id(),
                    .duration = new_job.job_duration,
                    .deadline = sim()->time() + task->period()});

                if (task->has_server() && (task->server()->state() == Server::State::Ready ||
                                           task->server()->state() == Server::State::Running)) {
                        task->server()->scheduler()->on_job_arrival(task, new_job.job_duration);
                }
                else {
                        need_to_place_task(new_job);
                }
        }

        for (const auto& resch : rescheds_) {
                resch->call_resched();
        }
        rescheds_.clear();
}

} // namespace allocators
