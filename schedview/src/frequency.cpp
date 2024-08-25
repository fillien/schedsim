#include "frequency.hpp"
#include <protocols/traces.hpp>

#include <iostream>
#include <map>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void outputs::frequency::print_frequency_changes(
    const std::multimap<double, protocols::traces::trace>& input)
{
        using namespace protocols::traces;
        std::vector<std::pair<double, double>> frequency_changes;

        double last_freq{0};

        for (const auto& [timestamp, tra] : input) {
                std::visit(
                    overloaded{
                        [&](frequency_update evt) {
                                frequency_changes.push_back({timestamp, last_freq});
                                std::cout << timestamp << ' ' << last_freq << '\n';
                                last_freq = evt.frequency;
                                std::cout << timestamp << ' ' << last_freq << '\n';
                                frequency_changes.push_back({timestamp, last_freq});
                        },
                        [&](sim_finished) {
                                std::cout << timestamp << ' ' << last_freq << '\n';
                                frequency_changes.push_back({timestamp, last_freq});
                        },
                        [](auto) {}},
                    tra);
        }
}
