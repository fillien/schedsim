#include "energy.hpp"
#include <protocols/traces.hpp>

#include <map>
#include <matplot/matplot.h>
#include <utility>
#include <variant>
#include <vector>

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

auto compute_power(const std::size_t& nb_active_cores, const double& frequency) -> double
{
        constexpr double POLYNUM{2};
        constexpr double PACKAGE_CONSUMPTION{1};
        /// TODO : Fix the power model
        return POLYNUM * frequency * static_cast<double>(nb_active_cores) + PACKAGE_CONSUMPTION;
}

auto parse_power_consumption(const std::multimap<double, protocols::traces::trace>& input)
    -> std::vector<std::pair<double, double>>
{
        std::vector<std::pair<double, double>> power_consumption;

        std::size_t current_active_cores{0};
        double current_freq{0};
        double current_power{0};
        double last_timestamp{0};

        for (const auto& [timestamp, tra] : input) {
                if (timestamp > last_timestamp) {
                        power_consumption.push_back({last_timestamp, current_power});
                        current_power = compute_power(current_active_cores, current_freq);
                        power_consumption.push_back({last_timestamp, current_power});
                        last_timestamp = timestamp;
                }

                std::visit(
                    overloaded{
                        [&](protocols::traces::proc_activated) { current_active_cores++; },
                        [&](protocols::traces::proc_idled) { current_active_cores--; },
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
                        double delta{timestamp - last_timestamp};
                        energy_timestamps.push_back(timestamp);
                        cumulative_energy += delta * value;
                        energy_measures.push_back(cumulative_energy);
                        last_timestamp = timestamp;
                }
        }

        std::cout << "Total energy consumption: " << cumulative_energy << std::endl;

        namespace plt = matplot;

        plt::plot(power_timestamps, power_measures);
        plt::title("Power Consumption Over Time");
        plt::xlabel("Time");
        plt::ylabel("Power Consumption");
        plt::hold(plt::on);
        plt::plot(energy_timestamps, energy_measures)->use_y2(true);
        plt::y2label("Cumulative Energy Consumption");
        plt::title("Power and Cumulative Energy Consumption Over Time");

        plt::save("energy.svg");
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
