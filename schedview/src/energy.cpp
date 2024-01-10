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

auto get_power_consumption(const std::multimap<double, protocols::traces::trace>& input)
    -> std::vector<std::pair<double, double>>
{
        constexpr double CORE_CONSUMPTION_PER_TIME_UNIT = 1;

        std::vector<std::pair<double, double>> power_consumption;

        double current_power{0};
        double last_timestamp{0};

        for (const auto& [timestamp, tra] : input) {
                if (timestamp > last_timestamp) {
                        power_consumption.push_back({last_timestamp, current_power});
                        last_timestamp = timestamp;
                        power_consumption.push_back({last_timestamp, current_power});
                }

                std::visit(
                    overloaded{
                        [&current_power](protocols::traces::proc_activated) {
                                current_power += CORE_CONSUMPTION_PER_TIME_UNIT;
                        },
                        [&current_power](protocols::traces::proc_idled) {
                                current_power -= CORE_CONSUMPTION_PER_TIME_UNIT;
                        },
                        [](auto) {}},
                    tra);
        }

        return power_consumption;
}

void outputs::energy::plot(const std::multimap<double, protocols::traces::trace>& input)
{
        const auto power_consumption = get_power_consumption(input);

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

        namespace plt = matplot;

        plt::plot(power_timestamps, power_measures);
        plt::title("Power Consumption Over Time");
        plt::xlabel("Time");
        plt::ylabel("Power Consumption");
        plt::hold(plt::on);
        plt::plot(energy_timestamps, energy_measures)->use_y2(true);
        plt::y2label("Cumulative Energy Consumption");
        plt::title("Power and Cumulative Energy Consumption Over Time");

        plt::show();
}
