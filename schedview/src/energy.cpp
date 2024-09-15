#include "energy.hpp"
#include "energy_model.hpp"
#include <cstddef>
#include <fstream>
#include <protocols/traces.hpp>

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>
#include <cassert>

#include <filesystem>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto parse_power_consumption(const std::multimap<double, protocols::traces::trace>& input)
    -> std::vector<std::pair<double, double>>
{
        std::vector<std::pair<double, double>> power_consumption;

        std::set<std::size_t> active_cores;
        std::size_t current_active_cores{0};
        double current_freq{0};
        double current_power{0};
        double last_timestamp{0};

        bool first {true};

        for (const auto& [timestamp, tra] : input) {
                assert(timestamp >= last_timestamp);
                if (timestamp > last_timestamp) {
                        if (!first) {
                            first = false;
                            power_consumption.push_back({last_timestamp, current_power});
                        }
                        current_power = energy::compute_power(current_freq) *
                                        static_cast<double>(current_active_cores);
                        power_consumption.push_back({last_timestamp, current_power});
                        last_timestamp = timestamp;
                }

                std::visit(
                    overloaded{
                        [&](protocols::traces::proc_activated evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                        current_active_cores++;
                                }
                        },
                        [&](protocols::traces::proc_idled evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                        current_active_cores++;
                                }
                        },
                        [&](protocols::traces::proc_sleep evt) {
                                if (active_cores.contains(evt.proc_id)) {
                                        active_cores.erase(evt.proc_id);
                                        current_active_cores--;
                                }
                        },
                        [&](protocols::traces::frequency_update evt) {
                                current_freq = evt.frequency;
                        },
                        [&](protocols::traces::sim_finished) {
                            power_consumption.push_back({last_timestamp, current_power});
                            current_power = energy::compute_power(current_freq) *
                                static_cast<double>(current_active_cores);
                            power_consumption.push_back({last_timestamp, current_power});
                            last_timestamp = timestamp;
                        },
                        [](auto) {}},
                    tra);
        }

        return power_consumption;
}

void outputs::energy::plot(const std::multimap<double, protocols::traces::trace>& input)
{
        const auto power_consumption = parse_power_consumption(input);

        std::vector<double> energy_timestamps;
        std::vector<double> energy_measures;

        double last_timestamp{0};
        double cumulative_energy{0};

        for (const auto& [timestamp, value] : power_consumption) {
                std::cout << timestamp << std::endl;
                if (timestamp > last_timestamp) {
                        const double delta{timestamp - last_timestamp};
                        energy_timestamps.push_back(timestamp);
                        cumulative_energy += delta * value;
                        energy_measures.push_back(cumulative_energy);
                        last_timestamp = timestamp;
                }
        }

        const std::filesystem::path& file{"power.csv"};

        std::ofstream out(file);
        if (!out) { throw std::runtime_error("Unable to open file: " + file.string()); }
        out << "time power\n";
        for (const auto& [timestamp, value] : power_consumption) {
                out << timestamp << ' ' << value << '\n';
        }
        out.close();
}

void outputs::energy::print_energy_consumption(
    const std::multimap<double, protocols::traces::trace>& input)
{
        const auto power_consumption = parse_power_consumption(input);

        std::vector<double> energy_timestamps;
        std::vector<double> energy_measures;

        double last_timestamp{0};
        double cumulative_energy{0};

        for (const auto& [timestamp, value] : power_consumption) {
                if (timestamp > last_timestamp) {
                        double delta{timestamp - last_timestamp};
                        energy_timestamps.push_back(timestamp);
                        cumulative_energy += delta * value;
                        energy_measures.push_back(cumulative_energy);
                        last_timestamp = timestamp;
                }
        }

        std::cout << cumulative_energy << std::endl;
}
