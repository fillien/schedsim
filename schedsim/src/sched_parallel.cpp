#include "sched_parallel.hpp"
#include "engine.hpp"
#include "event.hpp"
#include "platform.hpp"
#include "processor.hpp"
#include "server.hpp"

#include <algorithm>
#include <bits/ranges_algo.h>
#include <bits/ranges_base.h>
#include <bits/ranges_util.h>
#include <cmath>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <ostream>
#include <ranges>
#include <vector>

namespace {
auto get_max_utilization(
    const std::vector<std::shared_ptr<server>>& servers, const double& new_utilization = 0)
    -> double
{
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
} // namespace

auto sched_parallel::processor_order(const processor& first, const processor& second) -> bool
{
        if (!first.has_server_running()) { return false; }
        if (!second.has_server_running()) { return true; }
        return deadline_order(*(first.get_server()), *(second.get_server()));
}

auto sched_parallel::get_inactive_bandwidth() const -> double
{
        const auto TOTAL_UTILIZATION{get_total_utilization()};
        const auto MAX_UTILIZATION{get_max_utilization(servers)};
        const auto NB_PROCS{static_cast<double>(get_nb_active_procs())};
        return NB_PROCS - (NB_PROCS - 1) * MAX_UTILIZATION - TOTAL_UTILIZATION;
}

auto sched_parallel::get_nb_active_procs(const double& new_utilization) const -> std::size_t
{
        constexpr double MIN_NB_PROCS{1};
        const auto MAX_NB_PROCS{static_cast<double>(sim()->get_platform()->processors.size())};
        const auto TOTAL_UTILIZATION{get_total_utilization() + new_utilization};
        const auto MAX_UTILIZATION{get_max_utilization(servers, new_utilization)};
        double nb_procs{std::ceil((TOTAL_UTILIZATION - MAX_UTILIZATION) / (1 - MAX_UTILIZATION))};

        nb_procs = std::clamp(nb_procs, MIN_NB_PROCS, MAX_NB_PROCS);
        return static_cast<std::size_t>(nb_procs);
}

auto sched_parallel::get_server_budget(const server& serv) const -> double
{
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.utilization() / bandwidth * (serv.relative_deadline - serv.virtual_time);
}

auto sched_parallel::get_server_virtual_time(const server& serv, const double& running_time)
    -> double
{
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.virtual_time + bandwidth / serv.utilization() * running_time;
}

auto sched_parallel::admission_test(const task& new_task) const -> bool
{
        const auto NB_PROCS{static_cast<double>(get_nb_active_procs(new_task.utilization))};
        const auto U_MAX{get_max_utilization(servers, new_task.utilization)};
        const auto NEW_TOTAL_UTILIZATION{get_total_utilization() + new_task.utilization};
        return (NEW_TOTAL_UTILIZATION <= (NB_PROCS - (NB_PROCS - 1) * U_MAX));
}

void sched_parallel::on_active_utilization_updated()
{

}

void sched_parallel::custom_scheduler()
{
        update_running_servers();

        while (true) {
                // refresh active servers list
                auto ready_servers =
                    servers | std::views::filter(from_shared<server>(is_ready_server));

                // Check if there are servers in ready or running state
                if (std::distance(std::begin(ready_servers), std::end(ready_servers)) == 0) {
                        break;
                }

                // Get the server with the earliest deadline
                // Get the processeur that is idle or with the maximum deadline
                auto highest_priority_server =
                    std::ranges::min(ready_servers, from_shared<server>(deadline_order));
                auto leastest_priority_processor = std::ranges::max(
                    sim()->get_platform()->processors, from_shared<processor>(processor_order));

                if (!leastest_priority_processor->has_server_running() ||
                    deadline_order(
                        *highest_priority_server, *leastest_priority_processor->get_server())) {
                        resched_proc(leastest_priority_processor, highest_priority_server);
                }
                else {
                        break;
                }
        }

        // Set next job finish or budget exhausted event for each proc with a task
        for (auto proc : sim()->get_platform()->processors) {
                if (proc->has_server_running()) {
                        cancel_alarms(*proc->get_server());
                        set_alarms(proc->get_server());
                }
        }
}
