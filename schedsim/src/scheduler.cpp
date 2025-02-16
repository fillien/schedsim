#include <algorithm>
#include <cassert>
#include <engine.hpp>
#include <entity.hpp>
#include <event.hpp>
#include <iterator>
#include <map>
#include <memory>
#include <platform.hpp>
#include <processor.hpp>
#include <protocols/traces.hpp>
#include <ranges>
#include <scheduler.hpp>
#include <server.hpp>
#include <stdexcept>
#include <task.hpp>
#include <variant>
#include <vector>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace scheds {

auto Scheduler::chip() const -> std::shared_ptr<Cluster> { return attached_cluster.lock(); }

void Scheduler::call_resched()
{
        sim()->add_trace(protocols::traces::Resched{});
        on_resched();
}

auto Scheduler::u_max() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (servers.empty()) { return 0.0; }

        const auto max_server = std::ranges::max_element(servers, [](const auto& a, const auto& b) {
                return a->utilization() < b->utilization();
        });

        return (*max_server)->utilization();
}

auto Scheduler::is_this_my_event(const events::Event& evt) -> bool
{
        using namespace events;

        auto matches_server = [&](const auto& serv) {
                return std::any_of(
                    servers.begin(), servers.end(), [&](const std::shared_ptr<Server>& first) {
                            return first->id() == serv->id();
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
        if (std::holds_alternative<JobArrival>(evt)) {
                const auto& [task, duration] = std::get<JobArrival>(evt);
                return task->has_server() && matches_server(task->server());
        }
        if (std::holds_alternative<TimerIsr>(evt)) { return true; }

        throw std::runtime_error("Unknown event");
}

void Scheduler::update_running_servers()
{
        for (const auto& proc : chip()->processors) {
                if (proc->has_task()) { update_server_times(proc->task()->server()); }
        }
};

auto Scheduler::is_running_server(const Server& serv) -> bool
{
        return serv.state() == Server::State::Running;
}

auto Scheduler::is_ready_server(const Server& serv) -> bool
{
        return serv.state() == Server::State::Ready;
}

auto Scheduler::has_job_server(const Server& serv) -> bool
{
        return (serv.state() == Server::State::Ready) || (serv.state() == Server::State::Running);
}

auto Scheduler::is_active_server(const Server& serv) -> bool
{
        return serv.state() != Server::State::Inactive;
}

auto Scheduler::get_total_utilization() const -> double
{
        double total_u{0};
        for (const auto& serv : servers) {
                total_u += serv->utilization();
        }
        return total_u;
}

/// Compare two servers and return true if the first have an highest priority
auto Scheduler::deadline_order(const Server& first, const Server& second) -> bool
{
        if (first.deadline() == second.deadline()) {
                if (first.state() == Server::State::Running) { return true; }
                if (second.state() == Server::State::Running) { return false; }
                return first.id() < second.id();
        }
        return first.deadline() < second.deadline();
}

auto Scheduler::get_active_bandwidth() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        double active_bandwidth{0};
        for (const auto& serv : servers) {
                if (is_active_server(*serv)) { active_bandwidth += serv->utilization(); }
        }
        return active_bandwidth;
}

void Scheduler::detach_server_if_needed(const std::shared_ptr<Task>& inactive_task)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto& future_list = sim()->future_list();
        // Check if there is a future job arrival
        auto search = std::ranges::find_if(future_list, [inactive_task](auto& evt) {
                if (const auto& new_task = std::get_if<events::JobArrival>(&evt.second)) {
                        return (new_task->task_of_job == inactive_task);
                }
                return false;
        });
        if (search == std::end(future_list)) {
                // If no job found, detach the server of the task
                std::erase(servers, inactive_task->server());
                inactive_task->clear_server();
                total_utilization -= inactive_task->utilization();
                Engine::round_zero(total_utilization);
        }
}

void Scheduler::handle(const events::Event& evt)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using namespace events;

        std::visit(
            [this](auto&& arg) {
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
                    else if constexpr (std::is_same_v<T, JobArrival>) {
                            on_job_arrival(arg.task_of_job, arg.job_duration);
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

void Scheduler::on_serv_inactive(const std::shared_ptr<Server>& serv)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        // If a job arrived during this turn, do not change state to inactive
        if (serv->cant_be_inactive()) { return; }

        serv->change_state(Server::State::Inactive);
        detach_server_if_needed(serv->task());
        on_active_utilization_updated();

        // Update running server if there is one
        auto running_servers = servers | std::views::filter(from_shared<Server>(is_running_server));
        for (auto& serv : running_servers) {
                update_server_times(serv);
        }

        sim()->sched()->call_resched(shared_from_this());
}

void Scheduler::on_job_arrival(const std::shared_ptr<Task>& new_task, const double& job_duration)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        sim()->add_trace(
            traces::JobArrival{new_task->id(), job_duration, sim()->time() + new_task->period()});

        if (!new_task->has_server()) {
                if (!admission_test(*new_task)) {
                        sim()->add_trace(traces::TaskRejected{new_task->id()});
                        return;
                }

                // Total utilization increased -> updates running servers
                update_running_servers();

                auto new_server = std::make_shared<Server>(sim());
                new_task->server(new_server);
                servers.push_back(new_server);
                total_utilization += new_task->utilization();
        }

        assert(new_task->has_server());

        // Set the task remaining execution time with the WCET of the job
        new_task->add_job(job_duration);

        if (new_task->server()->state() == Server::State::Inactive) {
                update_running_servers();
                new_task->server()->virtual_time(sim()->time());
        }

        if (new_task->server()->state() != Server::State::Ready &&
            new_task->server()->state() != Server::State::Running) {
                new_task->server()->change_state(Server::State::Ready);
                on_active_utilization_updated();
                sim()->sched()->call_resched(shared_from_this());
        }
}

void Scheduler::on_job_finished(const std::shared_ptr<Server>& serv, bool is_there_new_job)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using enum Server::State;

        assert(serv->state() != Inactive);
        sim()->add_trace(protocols::traces::JobFinished{serv->id()});

        // Update virtual time and remaining execution time
        update_server_times(serv);

        if (serv->task()->has_job()) {
                serv->task()->next_job();
                serv->postpone();
        }
        else if (is_there_new_job) {
                // Looking for another job arrival of the task at the same time
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
        sim()->sched()->call_resched(shared_from_this());
}

void Scheduler::on_serv_budget_exhausted(const std::shared_ptr<Server>& serv)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        sim()->add_trace(traces::ServBudgetExhausted{serv->id()});
        update_server_times(serv);

        // Check if the job as been completed at the same time
        if (serv->task()->remaining_time() > 0) {
                serv->postpone(); // If no, postpone the deadline
        }
        else {
                sim()->add_trace(traces::JobFinished{serv->id()}); // If yes, trace it
        }

        sim()->sched()->call_resched(shared_from_this());
}

void Scheduler::update_server_times(const std::shared_ptr<Server>& serv)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        assert(serv->state() == Server::State::Running);

        const double rt = serv->running_time();

        // Be careful about floating point computation near 0
        assert((serv->task()->remaining_time() - rt) >= -Engine::ZERO_ROUNDED);

        serv->virtual_time(get_server_virtual_time(*serv, rt));
        sim()->add_trace(traces::VirtualTimeUpdate{serv->task()->id(), serv->virtual_time()});

        serv->task()->consume_time(rt);
        serv->update_time();
}

void Scheduler::cancel_alarms(const Server& serv)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        /// @TODO replace with iterator inside server and/or server if performance needed.
        using namespace events;
        const auto tid = serv.id();
        sim()->remove_event([tid](const auto& entry) {
                if (const auto& evt = std::get_if<ServBudgetExhausted>(&entry.second)) {
                        return evt->serv->id() == tid;
                }
                if (const auto& evt = std::get_if<JobFinished>(&entry.second)) {
                        return evt->server_of_job->id() == tid;
                }
                return false;
        });
}

void Scheduler::set_alarms(const std::shared_ptr<Server>& serv)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        using namespace events;
        namespace traces = protocols::traces;
        const double new_budget{Engine::round_zero(get_server_budget(*serv))};
        const double remaining_time{Engine::round_zero(serv->task()->remaining_time())};

#ifdef TRACY_ENABLE
        TracyPlot("budget", new_budget);
#endif
        assert(new_budget >= 0);
        assert(remaining_time >= 0);

        sim()->add_trace(traces::ServBudgetReplenished{serv->id(), new_budget});

        if (new_budget < remaining_time) {
                sim()->add_event(ServBudgetExhausted{serv}, sim()->time() + new_budget);
        }
        else {
                sim()->add_event(JobFinished{serv}, sim()->time() + remaining_time);
        }
}

void Scheduler::resched_proc(
    const std::shared_ptr<Processor>& proc, const std::shared_ptr<Server>& server_to_execute)
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
        return std::clamp(nb_procs, 1.0, static_cast<double>(chip()->processors.size()));
}

} // namespace scheds
