#include "sched_parallel.hpp"
#include "engine.hpp"
#include "plateform.hpp"
#include "processor.hpp"

#include <algorithm>
#include <bits/ranges_algo.h>
#include <bits/ranges_base.h>
#include <bits/ranges_util.h>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <ranges>

auto sched_parallel::processor_order(const std::shared_ptr<processor>& first,
                                     const std::shared_ptr<processor>& second) -> bool {
        if (!first->has_server_running()) {
                return false;
        } else if (!second->has_server_running()) {
                return true;
        } else {
                return deadline_order(first->get_server(), second->get_server());
        }
}

auto sched_parallel::get_inactive_bandwidth() -> double {
        double inactive_bandwidth{0};
        for (auto serv : servers) {
                if (serv->current_state == server::state::inactive) {
                        inactive_bandwidth += serv->utilization();
                }
        }
        return inactive_bandwidth;
}

auto sched_parallel::get_nb_active_procs() -> int {
        size_t nb_active_procs{0};
        for (const auto& proc : sim()->current_plateform->processors) {
                if (proc->has_server_running()) {
                        nb_active_procs++;
                }
        }

        // TODO Confirme that active proc can't be lower than 1
        if (nb_active_procs < 1) {
                nb_active_procs = 1;
        }

        std::cout << "nb_active_procs: " << nb_active_procs << std::endl;
        return nb_active_procs;
}

auto sched_parallel::get_server_budget(const std::shared_ptr<server>& serv) -> double {
        const auto bw = 1 - (get_inactive_bandwidth() / get_nb_active_procs());
        const auto res = serv->utilization() / bw * (serv->relative_deadline - serv->virtual_time);
        std::cout << "server " << serv->id() << " budget: " << res << "width"
                  << "\ninactive_bandwidth: " << get_inactive_bandwidth()
                  << "\nactive_procs: " << get_nb_active_procs()
                  << "\nserver utilization:" << serv->utilization()
                  << "\nserver relative deadline: " << serv->relative_deadline
                  << "\nserver virtual time: " << serv->virtual_time << std::endl;
        return res;
}

auto sched_parallel::get_server_new_virtual_time(const std::shared_ptr<server>& serv,
                                                 const double& running_time) -> double {
        const auto bw = 1 - (get_inactive_bandwidth() / get_nb_active_procs());
        return serv->virtual_time + bw / serv->utilization() * running_time;
}

auto sched_parallel::admission_test(const std::shared_ptr<task>& new_task) -> bool { return true; }

void sched_parallel::custom_scheduler() {
        while (true) {

                // refresh active servers list
                auto active_servers = servers | std::views::filter(is_active_server);

                // Check if there are servers in ready or running state
                if (std::distance(active_servers.begin(), active_servers.end()) == 0) {
                        break;
                }

                // Get the server with the earliest deadline
                // Get the processeur that is idle or with the maximum deadline
                auto highest_priority_server = std::ranges::min(active_servers, deadline_order);
                auto leastest_priority_processor =
                    std::ranges::max(sim()->current_plateform->processors, processor_order);

                if (leastest_priority_processor->current_state == processor::state::idle ||
                    deadline_order(highest_priority_server,
                                   leastest_priority_processor->get_server())) {
                        std::cout << "proc " << leastest_priority_processor->id << " with server "
                                  << highest_priority_server->id() << "\n";

                        resched_proc(leastest_priority_processor, highest_priority_server);
                } else {
                        break;
                }
        }
}
