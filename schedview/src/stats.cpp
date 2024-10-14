#include "stats.hpp"
#include <cstddef>
#include <iterator>
#include <map>
#include <protocols/traces.hpp>
#include <set>
#include <stdexcept>
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
                if (std::holds_alternative<traces::task_preempted>(event)) { cpt++; }
        }

        return cpt;
}

auto count_nb_contextswitch(const logs_type& input) -> std::size_t
{
        namespace traces = protocols::traces;
        std::size_t cpt{0};

        for (const auto& [_, event] : input) {
                if (std::holds_alternative<traces::task_preempted>(event)) { cpt++; }
                else if (std::holds_alternative<traces::job_finished>(event)) {
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
                        [&](protocols::traces::serv_ready event) {
                                open_ready_state_zone(last_zone_entry, event.task_id, timestamp);
                        },
                        [&](protocols::traces::serv_running event) {
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
                if (std::holds_alternative<protocols::traces::sim_finished>(event)) {
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
                if (std::holds_alternative<traces::task_rejected>(event)) { cpt++; }
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
                        [&](protocols::traces::proc_activated evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                        cpt++;
                                }
                        },
                        [&](protocols::traces::proc_idled evt) {
                                if (!active_cores.contains(evt.proc_id)) {
                                        active_cores.insert(evt.proc_id);
                                        cpt++;
                                }
                        },
                        [&](protocols::traces::proc_sleep evt) {
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
                if (std::holds_alternative<traces::frequency_update>(event) &&
                    timestamp > last_timestamp) {
                        last_timestamp = timestamp;
                        cpt++;
                }
        }
        return cpt;
}

} // namespace outputs::stats
