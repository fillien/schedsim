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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <ranges>
#include <variant>
#include <vector>

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

void scheduler::update_running_servers()
{
        for (const auto& proc : sim()->get_platform()->processors) {
                if (proc->has_server_running()) { update_server_times(proc->get_server()); }
        }
};

auto scheduler::is_running_server(const server& serv) -> bool
{
        return serv.current_state == server::state::running;
}

auto scheduler::is_ready_server(const server& serv) -> bool
{
        return serv.current_state == server::state::ready;
}

auto scheduler::has_job_server(const server& serv) -> bool
{
        return (serv.current_state == server::state::ready) ||
               (serv.current_state == server::state::running);
}

auto scheduler::is_active_server(const server& serv) -> bool
{
        return serv.current_state != server::state::inactive;
}

auto scheduler::get_total_utilization() const -> double
{
        double total_u{0};
        for (const auto& serv : servers) {
                if (is_active_server(*serv)) { total_u += serv->utilization(); }
        }
        return total_u;
}

/// Compare two servers and return true if the first have an highest priority
auto scheduler::deadline_order(const server& first, const server& second) -> bool
{
        if (first.relative_deadline == second.relative_deadline) {
                if (first.current_state == server::state::running) { return true; }
                if (second.current_state == server::state::running) { return false; }
                return first.id() < second.id();
        }
        return first.relative_deadline < second.relative_deadline;
}

auto scheduler::get_active_bandwidth() const -> double
{
        double active_bandwidth{0};
        for (const auto& serv : servers) {
                if (is_active_server(*serv)) { active_bandwidth += serv->utilization(); }
        }
        return active_bandwidth;
}

void scheduler::detach_server_if_needed(const std::shared_ptr<task>& inactive_task)
{
        // Check if there is a future job arrival
        auto search = std::ranges::find_if(sim()->future_list, [inactive_task](auto& evt) {
                if (const auto& new_task = std::get_if<events::job_arrival>(&evt.second)) {
                        return (new_task->task_of_job == inactive_task);
                }
                return false;
        });
        if (search == std::ranges::end(sim()->future_list)) {
                // If no job found, detach the server of the task
                std::erase(servers, inactive_task->get_server());
                inactive_task->unset_server();
                total_utilization -= inactive_task->utilization;
                sim()->round_zero(total_utilization);
        }
}

void scheduler::handle(std::vector<events::event> evts)
{
        // Sort events according to event priority cf:get_priority function
        std::sort(std::begin(evts), std::end(evts), compare_events);

        // Reset flags
        this->need_resched = false;

        for (const auto& evt : evts) {
                /// TODO refactor to a visitor pattern
                if (std::holds_alternative<events::job_finished>(evt)) {
                        // Looking for JOB_ARRIVAL events at the same time for this server
                        const auto& [serv] = std::get<events::job_finished>(evt);
                        auto is_there_new_job{false};
                        for (auto future_evt : evts) {
                                if (const auto& job_evt =
                                        std::get_if<events::job_arrival>(&future_evt)) {
                                        is_there_new_job =
                                            (job_evt->task_of_job == serv->get_task());
                                }
                        }
                        on_job_finished(serv, is_there_new_job);
                }
                else if (std::holds_alternative<events::serv_budget_exhausted>(evt)) {
                        const auto& [serv] = std::get<events::serv_budget_exhausted>(evt);
                        on_serv_budget_exhausted(serv);
                }
                else if (std::holds_alternative<events::serv_inactive>(evt)) {
                        const auto& [serv] = std::get<events::serv_inactive>(evt);
                        on_serv_inactive(serv);
                }
                else if (std::holds_alternative<events::job_arrival>(evt)) {
                        const auto& [serv, job_duration] = std::get<events::job_arrival>(evt);
                        on_job_arrival(serv, job_duration);
                }
                else {
                        std::cerr << "Unknowned event" << std::endl;
                }
        }

        if (this->need_resched) {
                sim()->add_trace(protocols::traces::resched{});
                on_resched();
        }

        // Update platform state
        for (auto const& proc : sim()->get_platform()->processors) {
                proc->update_state();
        }
}

void scheduler::on_serv_inactive(const std::shared_ptr<server>& serv)
{
        // If a job arrived during this turn, do not change state to inactive
        if (serv->cant_be_inactive) { return; }

        serv->change_state(server::state::inactive);
        detach_server_if_needed(serv->get_task());
        on_active_utilization_updated();

        // Update running server if there is one
        auto running_servers = servers | std::views::filter(from_shared<server>(is_running_server));
        for (auto& serv : running_servers) {
                update_server_times(serv);
        }

        need_resched = true;
}

void scheduler::on_job_arrival(const std::shared_ptr<task>& new_task, const double& job_duration)
{
        namespace traces = protocols::traces;
        sim()->add_trace(
            traces::job_arrival{new_task->id, job_duration, sim()->get_time() + new_task->period});

        if (!new_task->has_server()) {
                if (!admission_test(*new_task)) {
                        sim()->add_trace(traces::task_rejected{new_task->id});
                        return;
                }

                // Total utilization increased -> updates running servers
                update_running_servers();

                auto new_server = std::make_shared<server>(sim());
                new_task->set_server(new_server);
                servers.push_back(new_server);
                total_utilization += new_task->utilization;
        }

        assert(new_task->has_server());

        // Set the task remaining execution time with the WCET of the job
        new_task->add_job(job_duration);

        if (new_task->get_server()->current_state == server::state::inactive) {
                update_running_servers();
                new_task->get_server()->virtual_time = sim()->get_time();
        }

        if (new_task->get_server()->current_state != server::state::ready &&
            new_task->get_server()->current_state != server::state::running) {
                new_task->get_server()->change_state(server::state::ready);
                on_active_utilization_updated();
                this->need_resched = true;
        }
}

void scheduler::on_job_finished(const std::shared_ptr<server>& serv, bool is_there_new_job)
{
        using enum server::state;

        assert(serv->current_state != inactive);
        sim()->add_trace(protocols::traces::job_finished{serv->id()});

        // Update virtual time and remaining execution time
        update_server_times(serv);

        if (serv->get_task()->has_job()) {
                serv->get_task()->next_job();
                serv->postpone();
        }
        else if (is_there_new_job) {
                // Looking for another job arrival of the task at the same time
                serv->postpone();
        }
        else {
                serv->get_task()->attached_proc->clear_server();

                if ((serv->virtual_time - sim()->get_time()) > 0 &&
                    serv->virtual_time < serv->relative_deadline) {
                        serv->change_state(non_cont);
                }
                else {
                        serv->change_state(inactive);
                        // TODO Manage server detach
                        detach_server_if_needed(serv->get_task());
                        on_active_utilization_updated();
                }
        }
        this->need_resched = true;
}

void scheduler::on_serv_budget_exhausted(const std::shared_ptr<server>& serv)
{
        namespace traces = protocols::traces;
        sim()->add_trace(traces::serv_budget_exhausted{serv->id()});
        update_server_times(serv);

        // Check if the job as been completed at the same time
        if (serv->get_task()->get_remaining_time() > 0) {
                serv->postpone(); // If no, postpone the deadline
        }
        else {
                sim()->add_trace(traces::job_finished{serv->id()}); // If yes, trace it
        }

        this->need_resched = true;
}

void scheduler::update_server_times(const std::shared_ptr<server>& serv)
{
        namespace traces = protocols::traces;
        assert(serv->current_state == server::state::running);

        const double running_time = sim()->get_time() - serv->last_update;

        // Be careful about floating point computation near 0
        assert((serv->get_task()->get_remaining_time() - running_time) >= -engine::ZERO_ROUNDED);

        serv->virtual_time = get_server_virtual_time(*serv, running_time);
        sim()->add_trace(traces::virtual_time_update{serv->get_task()->id, serv->virtual_time});

        serv->get_task()->consume_time(running_time);
        serv->last_update = sim()->get_time();
}

void scheduler::cancel_alarms(const server& serv)
{
        /// @TODO replace with iterator inside server and/or server if performance needed.
        using namespace events;
        const auto tid = serv.id();
        std::erase_if(sim()->future_list, [tid](const auto& entry) {
                if (const auto& evt = std::get_if<serv_budget_exhausted>(&entry.second)) {
                        return evt->serv->id() == tid;
                }
                if (const auto& evt = std::get_if<job_finished>(&entry.second)) {
                        return evt->server_of_job->id() == tid;
                }
                return false;
        });
}

void scheduler::set_alarms(const std::shared_ptr<server>& serv)
{
        using namespace events;
        namespace traces = protocols::traces;
        const double new_budget{sim()->round_zero(get_server_budget(*serv))};
        const double remaining_time{sim()->round_zero(serv->remaining_exec_time())};

        assert(new_budget >= 0);
        assert(remaining_time >= 0);

        sim()->add_trace(traces::serv_budget_replenished{serv->id(), new_budget});

        if (new_budget < remaining_time) {
                sim()->add_event(serv_budget_exhausted{serv}, sim()->get_time() + new_budget);
        }
        else {
                sim()->add_event(job_finished{serv}, sim()->get_time() + remaining_time);
        }
}

void scheduler::resched_proc(
    const std::shared_ptr<processor>& proc, const std::shared_ptr<server>& server_to_execute)
{
        namespace traces = protocols::traces;
        if (proc->has_server_running()) {
                cancel_alarms(*(proc->get_server()));
                sim()->add_trace(traces::task_preempted{proc->get_server()->get_task()->id});
                proc->get_server()->change_state(server::state::ready);
                proc->clear_server();
                on_active_utilization_updated();
        }

        if (server_to_execute->current_state != server::state::running) {
                server_to_execute->change_state(server::state::running);
        }

        proc->set_server(server_to_execute);
}
