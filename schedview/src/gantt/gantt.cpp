#include "gantt.hpp"
#include <iterator>
#include <protocols/hardware.hpp>
#include <protocols/traces.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <variant>

namespace {

using namespace outputs::gantt;

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto count_tasks(const std::multimap<double, protocols::traces::trace>& traces) -> std::size_t
{
        using namespace protocols::traces;

        std::size_t max_tid{0};

        /// @TODO If the task has no jobs she is not counted in the number of tasks
        for (const auto& tra : traces) {
                if (std::holds_alternative<job_arrival>(tra.second)) {
                        max_tid = std::max(max_tid, std::get<job_arrival>(tra.second).task_id);
                }
        }

        return max_tid;
}

auto get_last_timestamp(const std::multimap<double, protocols::traces::trace>& traces) -> double
{
        double last_timestamp{0};
        if (traces.rbegin() != traces.rend()) { last_timestamp = std::prev(traces.end())->first; }

        return last_timestamp;
}

void open_execution_zone(
    std::map<std::size_t, std::pair<double, std::size_t>>& start_times,
    double time,
    std::size_t tid,
    std::size_t cpu)
{
        if (auto search = start_times.find(tid); search == std::end(start_times)) {
                start_times.insert({tid, {time, cpu}});
        }
}

void close_execution_zone(
    std::map<std::size_t, std::pair<double, std::size_t>>& start_times,
    double stop,
    std::size_t tid,
    gantt& chart,
    double freq)
{
        if (auto search = start_times.find(tid); search != std::end(start_times)) {
                const auto start = search->second.first;
                const auto cpu = search->second.second;
                chart.commands.emplace_back(execution{tid, cpu, start, stop, freq});
                start_times.erase(tid);
        }
}

void open_extra_budget_zone(
    std::map<std::size_t, double>& extra_budget_times, double time, std::size_t tid)
{
        if (auto search = extra_budget_times.find(tid); search == std::end(extra_budget_times)) {
                extra_budget_times.insert({tid, time});
        }
}

void close_extra_budget_zone(
    std::map<std::size_t, double>& extra_budget_times, double time, std::size_t tid, gantt& chart)
{
        if (auto search = extra_budget_times.find(tid); search != std::end(extra_budget_times)) {
                chart.commands.emplace_back(active_non_cont{tid, search->second, time});
                extra_budget_times.erase(tid);
        }
}

void new_arrival(gantt& chart, double time, std::size_t tid)
{
        chart.commands.emplace_back(arrival{tid, time});
}

void new_deadline(gantt& chart, double time, std::size_t tid)
{
        chart.commands.emplace_back(deadline{tid, time});
}

void new_finished(gantt& chart, double time, std::size_t tid)
{
        chart.commands.emplace_back(finished{tid, time});
}

auto get_proc_id(
    const std::map<std::size_t, std::pair<double, std::size_t>> executions, std::size_t tid)
    -> std::size_t
{
        if (auto search = executions.find(tid); search != std::end(executions)) {
                return search->second.second;
        }
        throw std::out_of_range("Task not found");
}

}; // namespace

namespace outputs::gantt {
auto generate_gantt(
    const std::multimap<double, protocols::traces::trace>& logs,
    const protocols::hardware::hardware& platform) -> gantt
{
        namespace traces = protocols::traces;
        auto [plat_f_min, plat_f_max] =
            std::minmax_element(std::begin(platform.frequencies), std::end(platform.frequencies));

        const auto f_max{*plat_f_max};
        const auto f_min{(*plat_f_min == *plat_f_max ? 0 : *plat_f_min)};
        auto current_freq{f_max};
        auto normalize = [&](double freq) -> double { return (freq - f_min) / (f_max - f_min); };

        gantt chart;

        chart.nb_axis = count_tasks(logs);
        chart.duration = std::ceil(get_last_timestamp(logs));

        std::map<std::size_t, std::pair<double, std::size_t>> execution_times;
        std::map<std::size_t, double> extra_budget_times;

        for (const auto& tra : logs) {
                const auto& timestamp = tra.first;
                const auto& event = tra.second;
                std::visit(
                    overloaded{
                        [&](traces::job_arrival evt) {
                                new_arrival(chart, timestamp, evt.task_id);
                        },
                        [&](traces::serv_postpone evt) {
                                new_deadline(chart, evt.deadline, evt.task_id);
                        },
                        [&](traces::job_finished evt) {
                                new_finished(chart, timestamp, evt.task_id);
                        },
                        [&](traces::serv_ready evt) {
                                new_deadline(chart, evt.deadline, evt.task_id);
                                close_extra_budget_zone(
                                    extra_budget_times, timestamp, evt.task_id, chart);
                                close_execution_zone(
                                    execution_times,
                                    timestamp,
                                    evt.task_id,
                                    chart,
                                    normalize(current_freq));
                        },
                        [&](traces::task_scheduled evt) {
                                open_execution_zone(
                                    execution_times, timestamp, evt.task_id, evt.proc_id);
                        },
                        [&](traces::task_preempted evt) {
                                close_execution_zone(
                                    execution_times,
                                    timestamp,
                                    evt.task_id,
                                    chart,
                                    normalize(current_freq));
                        },
                        [&](traces::serv_non_cont evt) {
                                close_execution_zone(
                                    execution_times,
                                    timestamp,
                                    evt.task_id,
                                    chart,
                                    normalize(current_freq));
                                open_extra_budget_zone(extra_budget_times, timestamp, evt.task_id);
                        },
                        [&](traces::serv_inactive evt) {
                                close_execution_zone(
                                    execution_times,
                                    timestamp,
                                    evt.task_id,
                                    chart,
                                    normalize(current_freq));
                                close_extra_budget_zone(
                                    extra_budget_times, timestamp, evt.task_id, chart);
                        },
                        [&](traces::frequency_update evt) {
                                std::set<std::size_t> tasks;
                                std::transform(
                                    execution_times.begin(),
                                    execution_times.end(),
                                    std::inserter(tasks, tasks.end()),
                                    [](auto pair) { return pair.first; });

                                for (auto tid : tasks) {
                                        auto cpu = get_proc_id(execution_times, tid);
                                        close_execution_zone(
                                            execution_times,
                                            timestamp,
                                            tid,
                                            chart,
                                            normalize(current_freq));
                                        open_execution_zone(execution_times, timestamp, tid, cpu);
                                }

                                current_freq = evt.frequency;
                        },
                        [](auto) {}},
                    event);
        }

        return chart;
}
}; // namespace outputs::gantt
