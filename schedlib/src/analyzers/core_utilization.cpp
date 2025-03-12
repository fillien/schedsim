#include "stats.hpp"
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
auto get_per_core_utilization(const std::vector<std::pair<double, protocols::traces::trace>>& input)
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
                        [&](protocols::traces::ProcActivated event) {
                                last_activation.insert({event.proc_id, timestamp});
                        },
                        [&](protocols::traces::ProcIdled event) {
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

namespace outputs::stats {
void print_utilizations(const std::vector<std::pair<double, protocols::traces::trace>>& input)
{
        const auto utilizations = get_per_core_utilization(input);

        std::cout << "Per core utilization:\n";
        for (const auto& [proc_id, utilization] : utilizations) {
                std::cout << "  - CPU " << proc_id << ": " << std::setprecision(4) << utilization
                          << "%\n";
        }
}

auto outputs::stats::track_change_state(
    const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>
{
        using namespace protocols::traces;

        std::map<std::string, std::vector<std::any>> table;

        std::map<std::size_t, double> last_activation;

        double last_timestamp{-1};
        double total_changing_time{0};

        for (const auto& [timestamp, tra] : input) {
                if (timestamp > last_timestamp) { last_timestamp = timestamp; }
                std::visit(
                    overloaded{
                        [&](protocols::traces::ProcChange evt) {
                                if (!changing_cores.contains(evt.proc_id)) {
                                        changing_cores.insert(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::ProcActivated evt) {
                                if (changing_cores.contains(evt.proc_id)) {
                                        changing_cores.erase(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::ProcIdled evt) {
                                if (changing_cores.contains(evt.proc_id)) {
                                        changing_cores.erase(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::ProcSleep evt) {
                                if (changing_cores.contains(evt.proc_id)) {
                                        changing_cores.erase(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::SimFinished) { table["stop"].push_back(timestamp); },
                        [](auto) {}},
                    tra);
        }

        return table;
}

} // namespace outputs::stats
