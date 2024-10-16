#include "frequency.hpp"
#include <protocols/traces.hpp>

#include <iostream>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto outputs::frequency::track_frequency_changes(
    const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>
{
        using namespace protocols::traces;

        std::map<std::string, std::vector<std::any>> table;

        double last_freq{-1};

        for (const auto& [timestamp, tra] : input) {
                std::visit(
                    overloaded{
                        [&](frequency_update evt) {
                                if (timestamp > last_freq) {
                                        last_freq = evt.frequency;
                                        if (!table["freq"].empty()) {
                                                table["stop"].push_back(timestamp);
                                        }
                                        table["freq"].push_back(evt.frequency);
                                        table["start"].push_back(timestamp);
                                }
                        },
                        [&](sim_finished) { table["stop"].push_back(timestamp); },
                        [](auto) {}},
                    tra);
        }

        return table;
}
