#include "stats.hpp"
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <protocols/traces.hpp>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace {
auto get_per_core_utilization(std::multimap<double, protocols::traces::trace> input)
    -> std::map<std::size_t, double>
{
        std::map<std::size_t, double> last_activation;
        std::map<std::size_t, double> per_core_utilization;

        auto close_utilization_zone = [&last_activation, &per_core_utilization](
                                          const double& timestamp, std::size_t proc_id) {
                if (auto search = last_activation.find(proc_id);
                    search != std::end(last_activation)) {
                        double current_utilization{0};

                        if (per_core_utilization.contains(proc_id)) {
                                current_utilization = per_core_utilization.at(proc_id);
                        }

                        current_utilization += timestamp - (*search).second;
                        per_core_utilization.insert({proc_id, current_utilization});
                        last_activation.erase(proc_id);
                }
        };

        for (const auto& tra : input) {
                const auto& timestamp = tra.first;
                const auto& event = tra.second;
                std::visit(
                    overloaded{
                        [&](protocols::traces::proc_activated event) {
                                last_activation.insert({event.proc_id, timestamp});
                        },
                        [&](protocols::traces::proc_idled event) {
                                close_utilization_zone(timestamp, event.proc_id);
                        },
                        [](auto) {}},
                    event);
        }

        double last_timestamp{0};
        if (input.rbegin() != input.rend()) { last_timestamp = std::prev(input.end())->first; }

        for (auto& [_, util] : per_core_utilization) {
                util = util * 100 / last_timestamp;
        }

        return per_core_utilization;
}

void open_ready_state_zone(
    std::map<std::size_t, double>& last_zone_entry, const std::size_t tid, const double& timestamp)
{
        if (auto search = last_zone_entry.find(tid); search == std::end(last_zone_entry)) {
                last_zone_entry.insert({tid, timestamp});
        }
}

void close_ready_state_zone(
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
void print_utilizations(const std::multimap<double, protocols::traces::trace>& input)
{
        const auto utilizations = get_per_core_utilization(input);

        std::cout << "Per core utilization:\n";
        for (const auto& [proc_id, utilization] : utilizations) {
                std::cout << "  - CPU " << proc_id << ": " << std::setprecision(4) << utilization
                          << "%\n";
        }
}

void print_nb_preemption(const std::multimap<double, protocols::traces::trace>& input)
{
        namespace traces = protocols::traces;
        std::size_t cpt_preemptions{0};

        for (const auto& [_, event] : input) {
                if (std::holds_alternative<traces::task_preempted>(event)) { cpt_preemptions++; }
        }

        std::cout << cpt_preemptions << std::endl;
}

void print_nb_contextswitch(const std::multimap<double, protocols::traces::trace>& input)
{
        namespace traces = protocols::traces;
        std::size_t cpt_contextswitch{0};

        for (const auto& [_, event] : input) {
                if (std::holds_alternative<traces::task_preempted>(event)) { cpt_contextswitch++; }
                if (std::holds_alternative<traces::job_finished>(event)) { cpt_contextswitch++; }
        }

        std::cout << cpt_contextswitch << std::endl;
}

void print_average_waiting_time(const std::multimap<double, protocols::traces::trace>& input)
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
        std::cout << "Average Waiting Time: " << average_waiting_time << "\n";
}

void print_duration(const std::multimap<double, protocols::traces::trace>& input)
{
        for (const auto& tra : input) {
                const auto& timestamp = tra.first;
                const auto& event = tra.second;
                std::visit(
                    overloaded{
                        [&](protocols::traces::sim_finished event) {
                                std::cout << timestamp << std::endl;
                        },
                        [](auto) {}},
                    event);
        }
}


} // namespace outputs::stats
