#include "analyzers/stats.hpp"
#include "protocols/hardware.hpp"
#include "protocols/traces.hpp"
#include "simulator/platform.hpp"

#include <any>
#include <cstddef>
#include <iterator>
#include <list>
#include <map>
#include <print>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace {
auto open_ready_state_zone(
    std::map<std::size_t, double>& last_zone_entry, const std::size_t tid, const double& timestamp)
{
        if (auto search = last_zone_entry.find(tid); search == std::end(last_zone_entry)) {
                last_zone_entry.insert({tid, timestamp});
        }
}

auto close_ready_state_zone(
    std::map<std::size_t, double>& last_zone_entry,
    std::map<std::size_t, double>& waiting_times,
    const std::size_t tid,
    const double& timestamp)
{
        if (auto search = last_zone_entry.find(tid); search != std::end(last_zone_entry)) {
                double current_waiting_time{0};

                if (waiting_times.contains(tid)) { current_waiting_time = waiting_times.at(tid); }

                current_waiting_time += timestamp - (*search).second;
                waiting_times.insert({tid, current_waiting_time});
                last_zone_entry.erase(tid);
        }
}

} // namespace

namespace outputs::stats {

auto count_nb_preemption(const logs_type& input) -> std::size_t
{
        namespace traces = protocols::traces;
        std::size_t cpt{0};

        for (const auto& [_, event] : input) {
                if (std::holds_alternative<traces::TaskPreempted>(event)) { cpt++; }
        }

        return cpt;
}

auto count_nb_contextswitch(const logs_type& input) -> std::size_t
{
        namespace traces = protocols::traces;
        std::size_t cpt{0};

        for (const auto& [_, event] : input) {
                if (std::holds_alternative<traces::TaskPreempted>(event)) { cpt++; }
                else if (std::holds_alternative<traces::JobFinished>(event)) {
                        cpt++;
                }
        }

        return cpt;
}

auto count_average_waiting_time(const logs_type& input) -> double
{
        std::map<std::size_t, double> last_zone_entry;
        std::map<std::size_t, double> waiting_times;

        for (const auto& tra : input) {
                const auto& timestamp = tra.first;
                const auto& event = tra.second;
                std::visit(
                    overloaded{
                        [&](protocols::traces::ServReady event) {
                                open_ready_state_zone(last_zone_entry, event.task_id, timestamp);
                        },
                        [&](protocols::traces::ServRunning event) {
                                close_ready_state_zone(
                                    last_zone_entry, waiting_times, event.task_id, timestamp);
                        },
                        [](auto) {}},
                    event);
        }

        double waiting_sum{0};

        for (const auto& [tid, waiting] : waiting_times) {
                waiting_sum += waiting;
        }

        const double average_waiting_time{waiting_sum / static_cast<double>(waiting_times.size())};
        return average_waiting_time;
}

auto count_duration(const logs_type& input) -> double
{
        double duration{-1};
        for (const auto& [timestamp, event] : input) {
                if (std::holds_alternative<protocols::traces::SimFinished>(event)) {
                        duration = timestamp;
                }
        }
        if (duration < -1) { throw std::runtime_error("scenario duration is less than 0"); }
        return duration;
}

auto count_rejected(const logs_type& input) -> std::size_t
{
        namespace traces = protocols::traces;
        std::size_t cpt{0};

        for (const auto& [_, event] : input) {
                if (std::holds_alternative<traces::TaskRejected>(event)) { cpt++; }
        }

        return cpt;
}

auto count_cluster_migration(const logs_type& input) -> std::size_t
{
        namespace traces = protocols::traces;
        // map : tid, {nb_transition, last_cid}
        // std::unordered_map<std::size_t, std::pair<std::size_t, std::size_t>> cpts;
        std::unordered_map<std::size_t, std::size_t> last_cids;

        std::size_t cpt{0};

        for (const auto& [_, event] : input) {
                if (const auto* evt = std::get_if<traces::TaskPlaced>(&event)) {
                        auto cid = last_cids.find(evt->task_id);
                        if (cid != last_cids.end() && cid->second != evt->cluster_id) {
                                last_cids.insert_or_assign(evt->task_id, evt->cluster_id);
                                cpt++;
                        }
                }
        }

        return cpt;
}

auto count_possible_transition(const logs_type& input) -> std::size_t
{
        using namespace protocols::traces;

        std::unordered_map<std::size_t, std::size_t> cpts;
        for (const auto& [_, event] : input) {
                if (const auto* evt = std::get_if<TaskPlaced>(&event)) { cpts[evt->task_id]++; }
        }

        std::size_t sum = 0;
        for (const auto& [_, cpt] : cpts) {
                sum += cpt;
        }

        return sum;
}

auto count_arrivals(const logs_type& input) -> std::size_t
{
        namespace traces = protocols::traces;
        std::size_t cpt{0};

        for (const auto& [_, event] : input) {
                if (std::holds_alternative<traces::JobArrival>(event)) { cpt++; }
        }

        return cpt;
}

auto count_core_state_request(const logs_type& input) -> std::size_t
{
        std::set<std::size_t> active_cores;
        std::size_t cpt{0};

        for (const auto& [timestamp, tra] : input) {
                std::visit(
                    overloaded{
                        [&](protocols::traces::ProcActivated evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                        cpt++;
                                }
                        },
                        [&](protocols::traces::ProcIdled evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                        cpt++;
                                }
                        },
                        [&](protocols::traces::ProcSleep evt) {
                                if (active_cores.contains(evt.proc_id)) {
                                        active_cores.erase(evt.proc_id);
                                        cpt++;
                                }
                        },
                        [](auto) {}},
                    tra);
        }
        return cpt;
}

auto count_frequency_request(const logs_type& input) -> std::size_t
{
        namespace traces = protocols::traces;
        std::size_t cpt{0};
        double last_timestamp{-1};

        for (const auto& [timestamp, event] : input) {
                if (std::holds_alternative<traces::FrequencyUpdate>(event) &&
                    timestamp > last_timestamp) {
                        last_timestamp = timestamp;
                        cpt++;
                }
        }
        return cpt;
}

auto count_cores_utilization(const logs_type& input, const protocols::hardware::Hardware& hw)
    -> std::map<std::string, std::vector<std::any>>
{
        using namespace protocols::traces;
        // duration, utilization
        std::unordered_map<std::size_t, std::list<std::pair<double, double>>> utilization_series;
        std::unordered_map<std::size_t, double> last_util;
        std::unordered_map<std::size_t, double> last_times;

        double sim_duration = 0;

        const auto update = [&](const auto& timestamp, const auto& id) {
                if (last_times[id] < timestamp) {
                        utilization_series[id].emplace_back(
                            timestamp - last_times[id], last_util[id]);
                        last_times[id] = timestamp;
                }
        };

        for (const auto& tra : input) {
                const auto& [timestamp, event] = tra;
                if (const auto* evt = std::get_if<ServReady>(&event)) {
                        update(timestamp, evt->sched_id);
                        last_util[evt->sched_id] += evt->utilization;
                }
                else if (const auto* evt = std::get_if<ServInactive>(&event)) {
                        update(timestamp, evt->sched_id);
                        last_util[evt->sched_id] -= evt->utilization;
                }
                else if (std::holds_alternative<protocols::traces::SimFinished>(event)) {
                        sim_duration = timestamp;
                }
        }

        std::vector<std::any> cluster_ids;
        std::vector<std::any> square_utils;
        for (std::size_t i = 1; i <= hw.clusters.size(); ++i) {
                cluster_ids.emplace_back(i);
                double core_util = 0;
                for (const auto& [duration, utilization] : utilization_series[i]) {
                        core_util += duration * utilization;
                }
                square_utils.emplace_back(core_util / sim_duration);
        }

        std::map<std::string, std::vector<std::any>> result;
        result["cluster_id"] = std::move(cluster_ids);
        result["util"] = std::move(square_utils);
        return result;
}

} // namespace outputs::stats
