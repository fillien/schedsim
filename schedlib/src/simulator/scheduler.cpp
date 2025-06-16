#include <algorithm>
#include <cassert>
#include <iterator>
#include <map>
#include <memory>
#include <protocols/traces.hpp>
#include <ranges>
#include <simulator/engine.hpp>
#include <simulator/entity.hpp>
#include <simulator/event.hpp>
#include <simulator/platform.hpp>
#include <simulator/processor.hpp>
#include <simulator/scheduler.hpp>
#include <simulator/server.hpp>
#include <simulator/task.hpp>
#include <stdexcept>
#include <variant>
#include <vector>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace scheds {

auto Scheduler::chip() const -> std::shared_ptr<Cluster> { return attached_cluster_.lock(); }

auto Scheduler::call_resched() -> void
{
        sim()->add_trace(protocols::traces::Resched{});
        on_resched();
}

auto Scheduler::u_max() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (servers_.empty()) { return 0.0; }

        const auto max_server =
            std::ranges::max_element(servers_, [](const auto& first, const auto& second) -> bool {
                    return first->utilization() < second->utilization();
            });
        return ((*max_server)->utilization() * cluster()->scale_speed()) / cluster()->perf();
}

auto Scheduler::is_this_my_event(const events::Event& evt) -> bool
{
        using namespace events;

        // Returns true if the server from the event is in the scheduler's server list.
        const auto matches_server = [this](const std::shared_ptr<Server>& serv) -> bool {
                return std::ranges::any_of(
                    servers_, [&serv](const std::shared_ptr<Server>& s) -> bool {
                            return s == serv;
                    });
        };

        if (std::holds_alternative<JobFinished>(evt)) {
                return matches_server(std::get<JobFinished>(evt).server_of_job);
        }
        if (std::holds_alternative<ServBudgetExhausted>(evt)) {
                return matches_server(std::get<ServBudgetExhausted>(evt).serv);
        }
        if (std::holds_alternative<ServInactive>(evt)) {
                return matches_server(std::get<ServInactive>(evt).serv);
        }
        if (std::holds_alternative<TimerIsr>(evt)) { return true; }
        if (!std::holds_alternative<JobArrival>(evt)) { throw std::runtime_error("Unknown event"); }
        return false;
}

auto Scheduler::update_running_servers() -> void
{
        // Update timing for servers that are running on any processor.
        for (const auto& proc : chip()->processors()) {
                if (proc->has_task()) {
                        // std::print(
                        //     "sched={0}, serv={1}, proc={2}\n",
                        //     cluster()->id(),
                        //     proc->task()->id(),
                        //     proc->id());
                        assert(proc->task()->has_server());
                        update_server_times(proc->task()->server());
                }
        }
}

auto Scheduler::has_job_server(const Server& serv) -> bool
{
        return serv.state() == Server::State::Ready || serv.state() == Server::State::Running;
}

auto Scheduler::total_utilization() const -> double
{
        double total{0.0};
        for (const auto& serv : servers_) {
                total += serv->utilization();
        }
        return (total * cluster()->scale_speed()) / cluster()->perf();
}

auto Scheduler::deadline_order(const Server& first, const Server& second) -> bool
{
        if (first.deadline() == second.deadline()) {
                // If deadlines are equal, running servers have higher priority.
                if (first.state() == Server::State::Running) { return true; }
                if (second.state() == Server::State::Running) { return false; }
                return first.id() < second.id();
        }
        return first.deadline() < second.deadline();
}

auto Scheduler::active_bandwidth() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        double bandwidth{0.0};
        for (const auto& serv : servers_) {
                if ((*serv).state() != Server::State::Inactive) {
                        bandwidth += serv->utilization();
                }
        }
        return (bandwidth * cluster()->scale_speed()) / cluster()->perf();
}

auto Scheduler::detach_server_if_needed(const std::shared_ptr<Task>& inactive_task) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto& future_list = sim()->future_list();

        if (inactive_task->has_server()) {
                // Look for any future job arrival associated with this task.
                const auto search =
                    std::ranges::find_if(future_list, [inactive_task](const auto& entry) -> bool {
                            if (const auto new_task =
                                    std::get_if<events::JobArrival>(&entry.second)) {
                                    return new_task->task_of_job == inactive_task;
                            }
                            return false;
                    });

                if (search == std::end(future_list)) {
                        // No pending job arrival: detach the task's server.
                        std::erase(servers_, inactive_task->server());
                        inactive_task->clear_server();
                        total_utilization_ -=
                            (inactive_task->utilization() * cluster()->scale_speed()) /
                            cluster()->perf();
                        Engine::round_zero(total_utilization_);
                }
                // else {
                //         std::erase(servers_, inactive_task->server());
                //         inactive_task->clear_server();
                //         total_utilization_ -=
                //             (inactive_task->utilization() * cluster()->scale_speed()) /
                //             cluster()->perf();
                //         Engine::round_zero(total_utilization_);
                // }
        }
        else {
                // std::print("erase serv={0}", inactive_task->server()->id());
                std::erase(servers_, inactive_task->server());
                total_utilization_ -=
                    (inactive_task->utilization() * cluster()->scale_speed()) / cluster()->perf();
                Engine::round_zero(total_utilization_);
        }
}

auto Scheduler::handle(const events::Event& evt) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using namespace events;
        std::visit(
            [this](auto&& arg) -> void {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, JobFinished>) {
                            on_job_finished(arg.server_of_job, arg.is_there_new_job);
                    }
                    else if constexpr (std::is_same_v<T, ServBudgetExhausted>) {
                            on_serv_budget_exhausted(arg.serv);
                    }
                    else if constexpr (std::is_same_v<T, ServInactive>) {
                            on_serv_inactive(arg.serv);
                    }
                    else if constexpr (std::is_same_v<T, TimerIsr>) {
                            arg.target_timer->fire();
                    }
                    else {
                            throw std::runtime_error("Unknown event");
                    }
            },
            evt);
}

auto Scheduler::on_serv_inactive(const std::shared_ptr<Server>& serv) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        // If the server cannot be marked inactive, do nothing.
        if (serv->cant_be_inactive()) { return; }

        // std::print("on_serv_inactive serv={0} to inactive\n", serv->id());
        serv->change_state(Server::State::Inactive);
        if (serv->been_migrated) {
                std::erase(servers_, serv);
                total_utilization_ -=
                    (serv->utilization() * cluster()->scale_speed()) / cluster()->perf();
                Engine::round_zero(total_utilization_);
        }
        else {
                detach_server_if_needed(serv->task());
        }
        on_active_utilization_updated();

        // Update timing for all running servers.
        for (const auto& itr :
             servers_ | std::views::filter([](const std::shared_ptr<Server>& serv) {
                     return serv->state() == Server::State::Running;
             })) {
                update_server_times(itr);
        }

        // Trigger a rescheduling.
        sim()->alloc()->call_resched(shared_from_this());
}

auto Scheduler::on_job_arrival(const std::shared_ptr<Task>& new_task, const double& job_duration)
    -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        namespace traces = protocols::traces;

        if (!new_task->has_server() ||
            (new_task->has_server() && new_task->server()->scheduler() != shared_from_this())) {
                if (!admission_test(*new_task)) {
                        sim()->add_trace(traces::TaskRejected{new_task->id()});
                        return;
                }

                // Update servers since the total utilization increases.
                update_running_servers();

                const auto new_server = std::make_shared<Server>(sim(), shared_from_this());
                new_task->server(new_server);
                servers_.push_back(new_server);
                total_utilization_ +=
                    (new_task->utilization() * cluster()->scale_speed()) / cluster()->perf();
        }

        assert(new_task->has_server());

        // Set the task's remaining execution time to the WCET of the job.
        new_task->add_job(job_duration);

        if (new_task->server()->state() == Server::State::Inactive) {
                update_running_servers();
                new_task->server()->virtual_time(sim()->time());
        }

        if (new_task->server()->state() != Server::State::Ready &&
            new_task->server()->state() != Server::State::Running) {
                new_task->server()->change_state(Server::State::Ready);
                on_active_utilization_updated();
                sim()->alloc()->call_resched(shared_from_this());
        }
}

auto Scheduler::on_job_finished(const std::shared_ptr<Server>& serv, bool is_there_new_job) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using enum Server::State;
        assert(serv->state() != Inactive);
        sim()->add_trace(protocols::traces::JobFinished{serv->id()});

        // Update virtual time and execution time.
        update_server_times(serv);

        if (serv->task()->has_job()) {
                serv->task()->next_job();
                serv->postpone();
        }
        else if (is_there_new_job) {
                // Another job arrival is expected at the same time.
                serv->postpone();
        }
        else {
                serv->task()->proc()->clear_task();

                if ((serv->virtual_time() - sim()->time()) > 0 &&
                    serv->virtual_time() < serv->deadline()) {
                        serv->change_state(NonCont);
                }
                else {
                        serv->change_state(Inactive);
                        detach_server_if_needed(serv->task());
                        on_active_utilization_updated();
                }
        }
        sim()->alloc()->call_resched(shared_from_this());
}

auto Scheduler::on_serv_budget_exhausted(const std::shared_ptr<Server>& serv) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        sim()->add_trace(
            traces::ServBudgetExhausted{.sched_id = cluster()->id(), .task_id = serv->id()});
        update_server_times(serv);

        // If the job is not yet complete, postpone the deadline.
        if (serv->task()->remaining_time() > 0) { serv->postpone(); }
        else {
                sim()->add_trace(traces::JobFinished{serv->id()});
        }
        sim()->alloc()->call_resched(shared_from_this());
}

auto Scheduler::update_server_times(const std::shared_ptr<Server>& serv) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        assert(serv->state() == Server::State::Running);

        const double rt = serv->running_time();
        // Ensure numerical stability.
        assert((serv->task()->remaining_time() - rt) >= -Engine::ZERO_ROUNDED);

        serv->virtual_time(server_virtual_time(*serv, rt));
        sim()->add_trace(traces::VirtualTimeUpdate{
            .task_id = serv->task()->id(), .virtual_time = serv->virtual_time()});
        serv->task()->consume_time(rt);
        serv->update_time();
}

auto Scheduler::cancel_alarms(const Server& serv) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using namespace events;
        const auto tid = serv.id();
        sim()->remove_event([tid](const auto& entry) -> bool {
                if (const auto evt = std::get_if<ServBudgetExhausted>(&entry.second)) {
                        return evt->serv->id() == tid;
                }
                if (const auto evt = std::get_if<JobFinished>(&entry.second)) {
                        return evt->server_of_job->id() == tid;
                }
                return false;
        });
}

auto Scheduler::activate_alarms(const std::shared_ptr<Server>& serv) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using namespace events;
        namespace traces = protocols::traces;
        const double new_budget = Engine::round_zero(server_budget(*serv));
        const double remaining_time = Engine::round_zero(serv->task()->remaining_time());

        assert(new_budget >= 0);
        assert(remaining_time >= 0);

        sim()->add_trace(traces::ServBudgetReplenished{
            .sched_id = cluster()->id(), .task_id = serv->id(), .budget = new_budget});

        if (new_budget < remaining_time) {
                sim()->add_event(ServBudgetExhausted{serv}, sim()->time() + new_budget);
        }
        else {
                sim()->add_event(
                    JobFinished{.server_of_job = serv}, sim()->time() + remaining_time);
        }
}

auto Scheduler::resched_proc(
    const std::shared_ptr<Processor>& proc, const std::shared_ptr<Server>& server_to_execute)
    -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;

        if (proc->has_task()) {
                cancel_alarms(*(proc->task()->server()));
                sim()->add_trace(traces::TaskPreempted{proc->task()->id()});
                proc->task()->server()->change_state(Server::State::Ready);
                proc->clear_task();
        }

        if (server_to_execute->state() != Server::State::Running) {
                server_to_execute->change_state(Server::State::Running);
        }

        proc->task(server_to_execute->task());
}

auto Scheduler::clamp(const double& nb_procs) -> double
{
        return std::clamp(nb_procs, 1.0, static_cast<double>(chip()->processors().size()));
}

} // namespace scheds
