#include <engine.hpp>
#include <event.hpp>
#include <processor.hpp>
#include <protocols/traces.hpp>
#include <schedulers/parallel.hpp>
#include <server.hpp>

#include <algorithm>
#include <bits/ranges_algo.h>
#include <bits/ranges_util.h>
#include <iterator>
#include <memory>
#include <ranges>
#include <vector>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

auto sched_parallel::get_max_utilization(
    const std::vector<std::shared_ptr<server>>& servers,
    const double& new_utilization) const -> double
{

#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (std::distance(std::begin(servers), std::end(servers)) > 0) {
                auto u_max = std::max_element(
                    std::begin(servers),
                    std::end(servers),
                    [](const std::shared_ptr<server>& first,
                       const std::shared_ptr<server>& second) {
                            return (first->utilization() < second->utilization());
                    });
                return std::max((*u_max)->utilization(), new_utilization);
        }
        return new_utilization;
}

auto sched_parallel::processor_order(const processor& first, const processor& second) -> bool
{
        using enum processor::state;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        if (!first.has_server_running()) { return (first.get_state() == idle); }
        if (!second.has_server_running()) { return (second.get_state() == sleep); }
        return deadline_order(*(first.get_server()), *(second.get_server()));
}

auto sched_parallel::get_inactive_bandwidth() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto TOTAL_UTILIZATION{get_total_utilization()};
        const auto MAX_UTILIZATION{get_max_utilization(servers)};
        const auto NB_PROCS{static_cast<double>(get_nb_active_procs())};
        return NB_PROCS - (NB_PROCS - 1) * MAX_UTILIZATION - TOTAL_UTILIZATION;
}

auto sched_parallel::get_nb_active_procs(const double& new_utilization) const -> std::size_t
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        return sim()->chip()->processors.size();
}

auto sched_parallel::get_server_budget(const server& serv) const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.utilization() / bandwidth * (serv.relative_deadline - serv.virtual_time);
}

auto sched_parallel::get_server_virtual_time(const server& serv, const double& running_time)
    -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.virtual_time + bandwidth / serv.utilization() * running_time;
}

auto sched_parallel::admission_test(const task& new_task) const -> bool
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto NB_PROCS{static_cast<double>(sim()->chip()->processors.size())};
        const auto U_MAX{get_max_utilization(servers, new_task.utilization)};
        const auto NEW_TOTAL_UTILIZATION{get_total_utilization() + new_task.utilization};
        return (NEW_TOTAL_UTILIZATION <= (NB_PROCS - (NB_PROCS - 1) * U_MAX));
}

void sched_parallel::remove_task_from_cpu(const std::shared_ptr<processor>& proc)
{
        using protocols::traces::task_preempted;
        if (proc->has_server_running()) {
                cancel_alarms(*(proc->get_server()));

                sim()->add_trace(task_preempted{proc->get_server()->get_task()->id});
                proc->get_server()->change_state(server::state::ready);
                proc->clear_server();
        }
}

void sched_parallel::on_resched()
{
        using std::ranges::empty;
        using std::ranges::max;
        using std::ranges::min;
        using std::views::filter;
        using enum processor::state;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        update_running_servers();

        update_platform();

        // Keep active procs and put to sleep others
        std::vector<std::shared_ptr<processor>> copy_chip = sim()->chip()->processors;
        std::sort(copy_chip.begin(), copy_chip.end(), from_shared<processor>(processor_order));
        auto middle = copy_chip.begin();
        std::advance(middle, get_nb_active_procs());

        auto it = copy_chip.begin();
        for (it; it != middle; ++it) {
                if ((*it)->get_state() == sleep) { (*it)->change_state(idle); }
        }

        for (it; it != copy_chip.end(); ++it) {
                remove_task_from_cpu((*it));
                (*it)->change_state(sleep);
        }

        // Place task using global EDF
        std::size_t cpt_scheduled_proc{0};
        while (cpt_scheduled_proc < get_nb_active_procs()) {
                // refresh active servers list
                auto ready_servers = servers | filter(from_shared<server>(is_ready_server));

                // Check if there are servers in ready or running state
                if (empty(ready_servers)) { break; }

                // Get the server with the earliest deadline
                // Get the processeur that is idle or with the maximum deadline
                auto highest_priority_server =
                    min(ready_servers, from_shared<server>(deadline_order));
                auto leastest_priority_processor =
                    min(sim()->chip()->processors, from_shared<processor>(processor_order));

                if (!leastest_priority_processor->has_server_running() ||
                    deadline_order(
                        *highest_priority_server, *leastest_priority_processor->get_server())) {
                        assert(leastest_priority_processor->get_state() != sleep);
                        resched_proc(leastest_priority_processor, highest_priority_server);

                        cpt_scheduled_proc++;
                }
                else {
                        break;
                }
        }

        // Set next job finish or budget exhausted event for each proc with a task
        for (auto proc : sim()->chip()->processors) {
                if (proc->get_state() == sleep) { continue; }
                else if (proc->has_server_running()) {
                        cancel_alarms(*proc->get_server());
                        set_alarms(proc->get_server());
                        proc->change_state(running);
                }
                else {
                        proc->change_state(idle);
                }
        }
}

void sched_parallel::update_platform() { sim()->chip()->set_freq(sim()->chip()->freq_max()); }
