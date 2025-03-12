#include "frequency.hpp"
#include <any>
#include <cstddef>
#include <print>
#include <protocols/traces.hpp>
#include <set>
#include <unordered_map>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
namespace {
auto remove_all_but_last_frequency_update_per_cluster(
    const std::vector<std::pair<double, protocols::traces::trace>>& original)
    -> std::vector<std::pair<double, protocols::traces::trace>>
{

        std::vector<std::pair<double, protocols::traces::trace>> result;
        size_t n = original.size();
        size_t i = 0;

        // Process the vector group by group
        while (i < n) {
                double current_timestamp = original[i].first;
                size_t start = i;
                size_t end = start;

                // Find the end of the current timestamp group
                while (end < n && original[end].first == current_timestamp) {
                        end++;
                }

                // Step 1: Map each cluster_id to the index of its last FrequencyUpdate
                std::unordered_map<size_t, size_t> last_index_map;
                for (size_t j = start; j < end; j++) {
                        if (const auto* fu = std::get_if<protocols::traces::FrequencyUpdate>(
                                &original[j].second)) {
                                last_index_map[fu->cluster_id] = j;
                        }
                }

                // Step 2: Build the result for this group
                for (size_t j = start; j < end; j++) {
                        const auto& pair = original[j];
                        if (const auto* fu =
                                std::get_if<protocols::traces::FrequencyUpdate>(&pair.second)) {
                                // Include FrequencyUpdate only if it's the last one for its
                                // cluster_id
                                if (j == last_index_map[fu->cluster_id]) { result.push_back(pair); }
                        }
                        else {
                                // Always include non-FrequencyUpdate events
                                result.push_back(pair);
                        }
                }

                // Move to the next timestamp group
                i = end;
        }

        return result;
}
} // namespace

auto outputs::frequency::track_frequency_changes(
    const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>
{
        using namespace protocols::traces;

        auto input_filtered = remove_all_but_last_frequency_update_per_cluster(input);

        std::map<std::size_t, std::map<std::string, std::vector<std::any>>> cluster_data;
        std::map<std::size_t, double> last_timestamp_per_cluster;

        for (const auto& [timestamp, tra] : input_filtered) {
                std::visit(
                    overloaded{
                        [&](FrequencyUpdate evt) {
                                std::size_t cluster = evt.cluster_id;
                                if (last_timestamp_per_cluster.find(cluster) ==
                                    last_timestamp_per_cluster.end()) {
                                        last_timestamp_per_cluster[cluster] = -1;
                                }

                                if (timestamp > last_timestamp_per_cluster[cluster]) {
                                        last_timestamp_per_cluster[cluster] = timestamp;

                                        if (!cluster_data[cluster]["freq"].empty()) {
                                                cluster_data[cluster]["stop"].push_back(timestamp);
                                        }

                                        cluster_data[cluster]["freq"].push_back(evt.frequency);
                                        cluster_data[cluster]["start"].push_back(timestamp);
                                        cluster_data[cluster]["cluster_id"].push_back(cluster);
                                }
                        },
                        [&](SimFinished) {
                                for (auto& [cluster, data] : cluster_data) {
                                        if (!data["freq"].empty() &&
                                            (data["stop"].empty() ||
                                             std::any_cast<double>(data["stop"].back()) <=
                                                 timestamp)) {
                                                cluster_data[cluster]["stop"].push_back(timestamp);
                                        }
                                }
                        },
                        [](auto) {}},
                    tra);
        }

        std::map<std::string, std::vector<std::any>> combined_table;
        for (const auto& [cluster, data] : cluster_data) {
                for (const auto& [key, values] : data) {
                        combined_table[key].insert(
                            combined_table[key].end(), values.begin(), values.end());
                }
        }

        return combined_table;
}

auto outputs::frequency::track_cores_changes(
    const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>
{
        using namespace protocols::traces;

        std::map<std::size_t, std::map<std::string, std::vector<std::any>>> cluster_data;
        std::map<std::size_t, std::set<std::size_t>> active_cores_per_cluster;

        size_t i = 0;
        while (i < input.size()) {
                double current_timestamp = input[i].first;
                std::set<std::size_t> affected_clusters;

                size_t j = i;
                while (j < input.size() && input[j].first == current_timestamp) {
                        const auto& [timestamp, tra] = input[j];
                        std::visit(
                            overloaded{
                                [&](ProcActivated evt) {
                                        std::size_t cluster = evt.cluster_id;
                                        if (!active_cores_per_cluster[cluster].contains(
                                                evt.proc_id)) {
                                                active_cores_per_cluster[cluster].insert(
                                                    evt.proc_id);
                                        }
                                        affected_clusters.insert(cluster);
                                },
                                [&](ProcIdled evt) {
                                        std::size_t cluster = evt.cluster_id;
                                        if (!active_cores_per_cluster[cluster].contains(
                                                evt.proc_id)) {
                                                active_cores_per_cluster[cluster].insert(
                                                    evt.proc_id);
                                        }
                                        affected_clusters.insert(cluster);
                                },
                                [&](ProcSleep evt) {
                                        std::size_t cluster = evt.cluster_id;
                                        if (active_cores_per_cluster[cluster].contains(
                                                evt.proc_id)) {
                                                active_cores_per_cluster[cluster].erase(
                                                    evt.proc_id);
                                        }
                                        affected_clusters.insert(cluster);
                                },
                                [&](SimFinished) {
                                        // Close all open intervals at this timestamp
                                        for (auto& [cluster, data] : cluster_data) {
                                                if (data["start"].size() > data["stop"].size()) {
                                                        data["stop"].push_back(current_timestamp);
                                                }
                                        }
                                },
                                [](auto) {}, // Ignore other event types
                            },
                            tra);
                        j++;
                }

                for (const auto& cluster : affected_clusters) {
                        if (cluster_data[cluster]["start"].empty()) {
                                // First interval for this cluster
                                cluster_data[cluster]["start"].push_back(current_timestamp);
                                cluster_data[cluster]["active_cores"].push_back(
                                    active_cores_per_cluster[cluster].size());
                                cluster_data[cluster]["cluster_id"].push_back(cluster);
                        }
                        else {
                                // Close the previous interval if open
                                if (cluster_data[cluster]["stop"].size() <
                                    cluster_data[cluster]["start"].size()) {
                                        cluster_data[cluster]["stop"].push_back(current_timestamp);
                                }
                                // Start a new interval with the updated number of active cores
                                cluster_data[cluster]["start"].push_back(current_timestamp);
                                cluster_data[cluster]["active_cores"].push_back(
                                    active_cores_per_cluster[cluster].size());
                                cluster_data[cluster]["cluster_id"].push_back(cluster);
                        }
                }

                // Move to the next timestamp
                i = j;
        }

        // Combine data from all clusters into a single table
        std::map<std::string, std::vector<std::any>> combined_table;
        for (const auto& [cluster, data] : cluster_data) {
                for (const auto& [key, values] : data) {
                        combined_table[key].insert(
                            combined_table[key].end(), values.begin(), values.end());
                }
        }

        return combined_table;
}

auto outputs::frequency::track_config_changes(
    const std::vector<std::pair<double, protocols::traces::trace>>& input)
    -> std::map<std::string, std::vector<std::any>>
{
        using namespace protocols::traces;

        std::map<std::string, std::vector<std::any>> table;

        std::set<std::size_t> active_cores;
        double last_timestamp{0};
        double last_freq{0};
        std::size_t last_cores{0};
        double new_freq{0};

        table["start"].push_back(last_timestamp);

        for (const auto& [timestamp, tra] : input) {
                if (timestamp > last_timestamp &&
                    (last_freq != new_freq || last_cores != active_cores.size())) {
                        table["stop"].push_back(timestamp);
                        table["freq"].push_back(new_freq);
                        table["active_cores"].push_back(active_cores.size());
                        table["start"].push_back(timestamp);
                        last_timestamp = timestamp;
                        last_freq = new_freq;
                        last_cores = active_cores.size();
                }
                std::visit(
                    overloaded{
                        [&](protocols::traces::ProcActivated evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::ProcIdled evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                }
                        },
                        [&](protocols::traces::ProcSleep evt) {
                                if (active_cores.contains(evt.proc_id)) {
                                        active_cores.erase(evt.proc_id);
                                }
                        },
                        // [&](protocols::traces::proc_change evt) {
                        //         if (active_cores.contains(evt.proc_id)) {
                        //                 active_cores.erase(evt.proc_id);
                        //         }
                        // },
                        [&](protocols::traces::FrequencyUpdate evt) { new_freq = evt.frequency; },
                        [&](protocols::traces::SimFinished) { table["stop"].push_back(timestamp); },
                        [](auto) {}},
                    tra);
        }

        return table;
}
