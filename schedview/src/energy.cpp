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

#include <filesystem>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto plot_energy(const std::vector<std::pair<double, double>>& measures)
    -> std::pair<std::vector<double>, std::vector<double>>
{
        double last_timestamp{0};
        double cumulative_energy{0};

        std::vector<double> energy_timestamps;
        std::vector<double> energy_measures;

        for (const auto& [timestamp, value] : measures) {
                if (timestamp > last_timestamp) {
                        double delta{timestamp - last_timestamp};
                        energy_timestamps.push_back(timestamp);
                        cumulative_energy += delta * value;
                        energy_measures.push_back(cumulative_energy);
                        last_timestamp = timestamp;
                }
        }
        return std::pair<std::vector<double>, std::vector<double>>{
            energy_timestamps, energy_timestamps};
}

auto parse_power_consumption(const std::multimap<double, protocols::traces::trace>& input)
    -> std::vector<std::pair<double, double>>
{
        std::vector<std::pair<double, double>> power_consumption;

        std::set<std::size_t> active_cores;
        std::size_t current_active_cores{0};
        double current_freq{0};
        double current_power{0};
        double last_timestamp{0};

        for (const auto& [timestamp, tra] : input) {
                if (timestamp > last_timestamp) {
                        power_consumption.push_back({last_timestamp, current_power});
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
                        [](auto) {}},
                    tra);
        }

        return power_consumption;
}

void outputs::energy::plot(const std::multimap<double, protocols::traces::trace>& input)
{
        const auto power_consumption = parse_power_consumption(input);

        std::vector<double> power_timestamps;
        std::vector<double> power_measures;

        for (const auto& point : power_consumption) {
                power_timestamps.push_back(point.first);
                power_measures.push_back(point.second);
        }

        std::vector<double> energy_timestamps;
        std::vector<double> energy_measures;

        double last_timestamp{0};
        double cumulative_energy{0};

        for (const auto& [timestamp, value] : power_consumption) {
                if (timestamp > last_timestamp) {
                        const double delta{timestamp - last_timestamp};
                        energy_timestamps.push_back(timestamp);
                        cumulative_energy += delta * value;
                        energy_measures.push_back(cumulative_energy);
                        last_timestamp = timestamp;
                }
        }

        const std::filesystem::path& file{""};
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

        std::vector<double> power_timestamps;
        std::vector<double> power_measures;

        for (const auto& point : power_consumption) {
                power_timestamps.push_back(point.first);
                power_measures.push_back(point.second);
        }

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
