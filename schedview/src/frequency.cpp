#include "frequency.hpp"
#include <protocols/traces.hpp>

#include <iostream>
#include <map>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void outputs::frequency::print_frequency_changes(
    const std::vector<std::pair<double, protocols::traces::trace>>& input)
{
        using namespace protocols::traces;

        double last_freq{0};

        std::cout << "timestamp freq\n";

        for (const auto& [timestamp, tra] : input) {
                std::visit(
                    overloaded{
                        [&](frequency_update evt) {
                                last_freq = evt.frequency;
                                std::cout << timestamp << ' ' << evt.frequency << '\n';
                        },
                        [&](sim_finished) { std::cout << timestamp << ' ' << last_freq << '\n'; },
                        [](auto) {}},
                    tra);
        }
}
