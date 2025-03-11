#include "energy.hpp"
#include "energy_model.hpp"
#include "protocols/hardware.hpp"
#include <cstddef>
#include <protocols/traces.hpp>

#include <cassert>
#include <set>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto parse_power_consumption(
    const std::vector<std::pair<double, protocols::traces::trace>>& input,
    const protocols::hardware::Hardware& hw) -> std::vector<std::pair<double, std::vector<double>>>
{
        std::vector<std::pair<double, std::vector<double>>> power_consumption;

        std::vector<std::set<std::size_t>> active_cores_per_cluster(hw.clusters.size());
        std::vector<double> freq_per_cluster(hw.clusters.size());

        for (const auto& [timestamp, tra] : input) {
                std::visit(
                    overloaded{
                        [&](protocols::traces::ProcActivated evt) {
                                active_cores_per_cluster[evt.cluster_id - 1].insert(evt.proc_id);
                        },
                        [&](protocols::traces::ProcIdled evt) {
                                active_cores_per_cluster[evt.cluster_id - 1].insert(evt.proc_id);
                        },
                        [&](protocols::traces::ProcSleep evt) {
                                active_cores_per_cluster[evt.cluster_id - 1].erase(evt.proc_id);
                        },
                        [&](protocols::traces::FrequencyUpdate evt) {
                                freq_per_cluster[evt.cluster_id - 1] = evt.frequency;
                        },
                        [](auto) {},
                    },
                    tra);

                std::vector<double> powers_per_cluster(hw.clusters.size());
                for (std::size_t cluster = 0; cluster < hw.clusters.size(); ++cluster) {
                        powers_per_cluster[cluster] =
                            static_cast<double>(active_cores_per_cluster[cluster].size()) *
                            energy::compute_power(freq_per_cluster[cluster], hw.clusters[cluster]);
                }
                power_consumption.emplace_back(timestamp, std::move(powers_per_cluster));
        }

        return power_consumption;
}

auto outputs::energy::compute_energy_consumption(
    const std::vector<std::pair<double, protocols::traces::trace>>& input,
    const protocols::hardware::Hardware& hw) -> std::map<std::string, std::vector<std::any>>
{
        const auto power_consumption = parse_power_consumption(input, hw);

        std::vector<double> energy_per_cluster(hw.clusters.size(), 0.0);

        if (!power_consumption.empty()) {
                for (std::size_t i = 0; i < power_consumption.size() - 1; ++i) {
                        double delta = power_consumption[i + 1].first - power_consumption[i].first;
                        if (delta > 0) {
                                for (std::size_t cluster = 0; cluster < hw.clusters.size();
                                     ++cluster) {
                                        energy_per_cluster[cluster] +=
                                            power_consumption[i].second[cluster] * delta;
                                }
                        }
                }
        }

        // Create vectors for the table columns
        std::vector<std::any> cluster_ids;
        std::vector<std::any> energies;

        // Populate the vectors
        for (std::size_t i = 0; i < hw.clusters.size(); ++i) {
                cluster_ids.push_back(i + 1);
                energies.push_back(energy_per_cluster[i]);
        }

        std::map<std::string, std::vector<std::any>> result;
        result["cluster_id"] = std::move(cluster_ids);
        result["energy_consumption"] = std::move(energies);

        return result;
}
