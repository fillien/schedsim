#include "gantt.hpp"
#include "traces.hpp"

#include <cmath>
#include <map>
#include <set>
#include <variant>

namespace {

using namespace outputs::gantt;

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto count_tasks(const std::multimap<double, traces::trace>& traces) -> std::size_t
{
        using namespace traces;

        std::set<std::size_t> cpt;

        for (const auto& tra : traces) {
                if (std::holds_alternative<job_arrival>(tra.second)) {
                        cpt.insert(std::get<job_arrival>(tra.second).task_id);
                }
        }

        return cpt.size();
}

auto get_last_timestamp(const std::multimap<double, traces::trace>& traces) -> double
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
    gantt& chart)
{
        if (auto search = start_times.find(tid); search != std::end(start_times)) {
                const auto start = search->second.first;
                const auto cpu = search->second.second;
                chart.commands.emplace_back(execution{tid, cpu, start, stop});
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
                chart.commands.emplace_back(
                    active_non_cont{tid, search->second, time - search->second});
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
}; // namespace

namespace outputs::gantt {
auto generate_gantt(const std::multimap<double, traces::trace>& logs) -> gantt
{
        gantt chart;

        chart.nb_axis = count_tasks(logs);
        chart.duration = std::ceil(get_last_timestamp(logs));

        std::map<std::size_t, std::pair<double, std::size_t>> execution_times;
        std::map<std::size_t, double> extra_budget_times;

        for (const auto& tra : logs) {
                const auto timestamp = tra.first;
                const auto event = tra.second;
                std::visit(
                    overloaded{
                        [&](traces::job_arrival evt) {
                                new_arrival(chart, timestamp, evt.task_id);
                        },
                        [&](traces::serv_postpone evt) {
                                new_deadline(chart, evt.deadline, evt.task_id);
                        },
                        [&](traces::serv_ready evt) {
                                new_deadline(chart, evt.deadline, evt.task_id);
                        },
                        [&](traces::task_scheduled evt) {
                                open_execution_zone(
                                    execution_times, timestamp, evt.task_id, evt.proc_id);
                        },
                        [&](traces::task_preempted evt) {
                                close_execution_zone(
                                    execution_times, timestamp, evt.task_id, chart);
                        },
                        [&](traces::serv_non_cont evt) {
                                close_execution_zone(
                                    execution_times, timestamp, evt.task_id, chart);
                                open_extra_budget_zone(extra_budget_times, timestamp, evt.task_id);
                        },
                        [&](traces::serv_inactive evt) {
                                close_execution_zone(
                                    execution_times, timestamp, evt.task_id, chart);
                                close_extra_budget_zone(
                                    extra_budget_times, timestamp, evt.task_id, chart);
                        },
                        [](auto) {}},
                    event);
        }

        return chart;
}
}; // namespace outputs::gantt
