#include "frequency.hpp"
#include <cstddef>
#include <protocols/traces.hpp>
#include <set>
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

        double last_timestamp{-1};

        for (const auto& [timestamp, tra] : input) {
                std::visit(
                    overloaded{
                        [&](frequency_update evt) {
                                if (timestamp > last_timestamp) {
                                        last_timestamp = timestamp;
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

auto outputs::frequency::track_cores_changes(
    const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>
{
        using namespace protocols::traces;

        std::map<std::string, std::vector<std::any>> table;

        std::set<std::size_t> active_cores;
        std::size_t last_cpt_active_cores{0};
        double last_timestamp{0};

        for (const auto& [timestamp, tra] : input) {
                if (timestamp > last_timestamp) {
                        last_timestamp = timestamp;
                        if (!table["active_cores"].empty()) { table["stop"].push_back(timestamp); }
                        table["active_cores"].push_back(active_cores.size());
                        table["start"].push_back(timestamp);
                }
                std::visit(
                    overloaded{
                        [&](protocols::traces::proc_activated evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::proc_idled evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::proc_sleep evt) {
                                if (active_cores.contains(evt.proc_id)) {
                                        active_cores.erase(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::sim_finished) {
                                table["stop"].push_back(timestamp);
                        },
                        [](auto) {}},
                    tra);
        }

        return table;
}
