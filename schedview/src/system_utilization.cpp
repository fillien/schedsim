#include "system_utilization.hpp"
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <protocols/traces.hpp>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace {
auto get_per_core_utilization(std::multimap<double, protocols::traces::trace> input)
    -> std::map<std::size_t, double>
{
        std::map<std::size_t, double> last_activation;
        std::map<std::size_t, double> per_core_utilization;

        auto close_utilization_zone = [&last_activation, &per_core_utilization](
                                          const double& timestamp, std::size_t proc_id) {
                if (auto search = last_activation.find(proc_id);
                    search != std::end(last_activation)) {
                        double current_utilization{0};

                        if (per_core_utilization.contains(proc_id)) {
                                current_utilization = per_core_utilization.at(proc_id);
                        }

                        current_utilization += timestamp - (*search).second;
                        per_core_utilization.insert({proc_id, current_utilization});
                        last_activation.erase(proc_id);
                }
        };

        for (const auto& tra : input) {
                const auto& timestamp = tra.first;
                const auto& event = tra.second;
                std::visit(
                    overloaded{
                        [&](protocols::traces::proc_activated event) {
                                last_activation.insert({event.proc_id, timestamp});
                        },
                        [&](protocols::traces::proc_idled event) {
                                close_utilization_zone(timestamp, event.proc_id);
                        },
                        [](auto) {}},
                    event);
        }

        double last_timestamp{0};
        if (input.rbegin() != input.rend()) { last_timestamp = std::prev(input.end())->first; }

        for (auto& [_, util] : per_core_utilization) {
                util = util * 100 / last_timestamp;
        }

        return per_core_utilization;
}
} // namespace

namespace outputs::sys_util {

void print_active_utilization(const std::multimap<double, protocols::traces::trace>& input)
{
        double current_active_utilization{0};
        // timestamp, active utilization
        // std::vector<std::pair<double, double>> utilization_over_time;

        std::cout << "0.0 0.0\n";
        for (const auto& tra : input) {
                const auto& [timestamp, event] = tra;

                std::visit(
                    overloaded{
                        [&](protocols::traces::serv_ready evt) {
                                current_active_utilization += evt.utilization;
                                std::cout << timestamp << ' ' << current_active_utilization << '\n';
                        },
                        [&](protocols::traces::serv_inactive evt) {
                                current_active_utilization -= evt.utilization;
                                std::cout << timestamp << ' ' << current_active_utilization << '\n';
                        },
                        [](auto) {}},
                    event);
        }
}
} // namespace outputs::sys_util
