#include "sched_parallel.hpp"
#include "engine.hpp"
#include "event.hpp"
#include "plateform.hpp"
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
#include <ranges>
#include <vector>

auto get_max_utilization(
    std::vector<std::shared_ptr<server>>& servers, const double& new_utilization = 0) -> double
{
        if (std::distance(std::begin(servers), std::end(servers)) > 0) {
                auto u_max = std::max_element(
                    std::begin(servers), std::end(servers),
                    [](const std::shared_ptr<server>& first,
                       const std::shared_ptr<server>& second) {
                            return (first->utilization() < second->utilization());
                    });
                return std::max((*u_max)->utilization(), new_utilization);
        }
        return new_utilization;
}

auto sched_parallel::processor_order(
    const std::shared_ptr<processor>& first, const std::shared_ptr<processor>& second) -> bool
{
        if (!first->has_server_running()) {
                return false;
        }
        if (!second->has_server_running()) {
                return true;
        }
        return deadline_order(first->get_server(), second->get_server());
}

auto sched_parallel::get_inactive_bandwidth() -> double
{
        double inactive_bandwidth{0};
        for (auto serv : servers) {
                if (serv->current_state == server::state::inactive) {
                        inactive_bandwidth += serv->utilization();
                }
        }
        return inactive_bandwidth;
}

auto sched_parallel::get_nb_active_procs(const double& new_utilization) -> int
{
        const int max_nb_procs = static_cast<int>(sim()->get_plateform()->processors.size());
        int nb_procs = std::ceil(
            ((get_total_utilization() + new_utilization) -
             get_max_utilization(servers, new_utilization)) /
            (1 - get_max_utilization(servers, new_utilization)));
        if (nb_procs > max_nb_procs) {
                nb_procs = max_nb_procs;
        }
        else if (nb_procs < 1) {
                nb_procs = 1;
        }
        std::cout << "Minimum nb procs: " << nb_procs << std::endl;
        return nb_procs;
}

auto sched_parallel::get_server_budget(const std::shared_ptr<server>& serv) -> double
{
        const auto bandwidth = 1 - (get_inactive_bandwidth() / get_nb_active_procs());
        return serv->utilization() / bandwidth * (serv->relative_deadline - serv->virtual_time);
}

auto sched_parallel::get_server_new_virtual_time(
    const std::shared_ptr<server>& serv, const double& running_time) -> double
{
        const auto bandwidth = 1 - (get_inactive_bandwidth() / get_nb_active_procs());
        return serv->virtual_time + bandwidth / serv->utilization() * running_time;
}

auto sched_parallel::admission_test([[maybe_unused]] const std::shared_ptr<task>& new_task) -> bool
{
        const auto m_procs = get_nb_active_procs(new_task->utilization);
        const auto u_max = get_max_utilization(servers, new_task->utilization);
        return (
            (get_total_utilization() + new_task->utilization) <= (m_procs - (m_procs - 1) * u_max));
}

void sched_parallel::custom_scheduler()
{
        for (auto proc : sim()->get_plateform()->processors) {
                if (proc->has_server_running()) {
                        update_server_times(proc->get_server());
                }
        }

        while (true) {
                // refresh active servers list
                auto ready_servers = servers | std::views::filter(is_ready_server);

                std::cout << "nb ready_servers: "
                          << std::distance(ready_servers.begin(), ready_servers.end()) << std::endl;

                // Check if there are servers in ready or running state
                if (std::distance(ready_servers.begin(), ready_servers.end()) == 0) {
                        break;
                }

                // Get the server with the earliest deadline
                // Get the processeur that is idle or with the maximum deadline
                auto highest_priority_server = std::ranges::min(ready_servers, deadline_order);
                auto leastest_priority_processor =
                    std::ranges::max(sim()->get_plateform()->processors, processor_order);

                std::cout << "server id: " << highest_priority_server->id()
                          << ", deadline: " << highest_priority_server->relative_deadline
                          << std::endl;

                if (!leastest_priority_processor->has_server_running() ||
                    deadline_order(
                        highest_priority_server, leastest_priority_processor->get_server())) {
                        resched_proc(leastest_priority_processor, highest_priority_server);
                }
                else {
                        break;
                }
        }

        // Set next job finish or budget exhausted event for each proc with a task
        for (auto proc : sim()->get_plateform()->processors) {
                if (proc->has_server_running()) {
                        const auto& serv = proc->get_server();

                        const auto proc_server_id = proc->get_server()->id();
                        std::erase_if(sim()->future_list, [proc_server_id](const auto& evt) {
                                if (std::holds_alternative<events::serv_budget_exhausted>(
                                        evt.second)) {
                                        const auto& [serv] =
                                            std::get<events::serv_budget_exhausted>(evt.second);
                                        if (serv->id() == proc_server_id) {
                                                std::cout << "REMOVE BUDGET_EXHAUSTED FOR S"
                                                          << serv->id() << " AT " << evt.first
                                                          << std::endl;
                                        }
                                        return serv->id() == proc_server_id;
                                }
                                if (std::holds_alternative<events::job_finished>(evt.second)) {
                                        const auto& [serv] =
                                            std::get<events::job_finished>(evt.second);
                                        if (serv->id() == proc_server_id) {
                                                std::cout << "REMOVE JOB_FINISHED FOR S"
                                                          << serv->id() << std::endl;
                                        }
                                        return serv->id() == proc_server_id;
                                }
                                return false;
                        });

                        // Compute the budget of the selected server
                        double new_server_budget = get_server_budget(serv);
                        double task_remaining_time = serv->remaining_exec_time();

                        assert(new_server_budget >= 0);
                        assert(task_remaining_time >= 0);

                        sim()->add_trace(events::serv_budget_replenished{serv, new_server_budget});

                        // Insert the next event about this server in the future list
                        if (new_server_budget < task_remaining_time) {
                                // Insert a budget exhausted event
                                sim()->add_event(
                                    events::serv_budget_exhausted{serv},
                                    sim()->get_time() + new_server_budget);
                                std::cout << "INSERT BUDGET_EXHAUSTED FOR S" << serv->id() << " AT "
                                          << sim()->get_time() + new_server_budget << std::endl;
                        }
                        else {
                                // Insert an end of task event
                                std::cout << "INSERT JOB_FINISHED FOR S" << serv->id() << std::endl;
                                sim()->add_event(
                                    events::job_finished{serv},
                                    sim()->get_time() + task_remaining_time);
                        }
                        std::cout << "S" << serv->id() << " on P" << proc->get_id() << std::endl;
                }
        }
}
