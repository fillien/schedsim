#include "sched_mono.hpp"
#include "engine.hpp"
#include <algorithm>
#include <memory>
#include <ranges>

auto sched_mono::is_in_runqueue(const std::shared_ptr<server>& current_server) -> bool {
        return is_ready_server(current_server) || is_running_server(current_server);
}

auto sched_mono::get_active_bandwidth() -> double {
        double active_bandwidth{0};
        for (auto serv : servers) {
                if (is_active_server(serv)) {
                        active_bandwidth += serv->utilization();
                }
        }
        return active_bandwidth;
}

auto sched_mono::get_server_budget(const std::shared_ptr<server>& serv) -> double {
        const double active_bw = get_active_bandwidth();
        return serv->utilization() / active_bw * (serv->relative_deadline - serv->virtual_time);
}

auto sched_mono::get_server_new_virtual_time(const std::shared_ptr<server>& serv,
                                             const double& running_time) -> double {
        const auto active_bw = get_active_bandwidth();
        return serv->virtual_time + running_time * (active_bw / serv->utilization());
}

auto sched_mono::admission_test(const std::shared_ptr<task>& new_task) -> bool {
        double active_utilisation{0};
        for (auto serv : servers) {
                if (serv->current_state != server::state::inactive) {
                        active_utilisation += serv->utilization();
                }
        }

        std::cout << "admission test : active utilisation = " << active_utilisation
                  << "; new_task utilisation = " << new_task->utilization << std::endl;

        return (new_task->utilization + active_utilisation) <= 1;
}

void sched_mono::custom_scheduler() {
        // Check if there is no active and running server
        auto runqueue_servers = servers | std::views::filter(is_in_runqueue);

        // Check if there are servers in ready state
        if (std::distance(runqueue_servers.begin(), runqueue_servers.end()) == 0) {
                std::cout << "ready server is empty" << std::endl;
                return;
        }

        auto highest_priority_server = std::ranges::min(runqueue_servers, deadline_order);
        if (proc->has_server_running()) {
                auto running_server = proc->get_server();
                if (deadline_order(highest_priority_server, running_server)) {
                        resched_proc(proc, highest_priority_server);
                }
        } else {
                resched_proc(proc, highest_priority_server);
        }
}
