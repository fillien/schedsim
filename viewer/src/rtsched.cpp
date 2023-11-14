#include "rtsched.hpp"
#include "trace.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace {
using namespace outputs::rtsched;

auto count_tasks(const input_data& traces) -> std::size_t
{
        using namespace traces;

        std::set<std::size_t> cpt;

        for (const auto& tra : traces) {
                if (std::holds_alternative<job_arrival>(tra.second)) {
                        cpt.insert(std::get<job_arrival>(tra.second).id);
                }
        }

        return cpt.size();
}

auto get_color(std::size_t cpu_id) -> std::string
{
        constexpr std::array colors{"red",       "green",  "blue", "cyan",  "magenta",
                                    "yellow",    "black",  "gray", "white", "darkgray",
                                    "lightgray", "brown",  "lime", "olive", "orange",
                                    "pink",      "purple", "teal", "violet"};
        return colors.at(cpu_id);
}

auto get_last_timestamp(const input_data& traces) -> double
{
        double last_timestamp{0};
        if (traces.rbegin() != traces.rend()) {
                last_timestamp = std::prev(traces.end())->first;
        }

        return last_timestamp;
}

void open_execution_zone(
    std::map<std::size_t, std::pair<double, std::size_t>>& start_times, double time,
    std::size_t tid, std::size_t cpu)
{
        if (auto search = start_times.find(tid); search == std::end(start_times)) {
                start_times.insert({tid, {time, cpu}});
        }
}

void close_execution_zone(
    std::map<std::size_t, std::pair<double, std::size_t>>& start_times, double stop,
    std::size_t tid, grid& grid)
{
        if (auto search = start_times.find(tid); search != std::end(start_times)) {
                const auto start = search->second.first;
                const auto cpu = search->second.second;
                grid.commands.emplace_back(outputs::rtsched::TaskExecution{tid, start, stop, cpu});
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
    std::map<std::size_t, double>& extra_budget_times, double time, std::size_t tid, grid& grid)
{
        if (auto search = extra_budget_times.find(tid); search != std::end(extra_budget_times)) {
                grid.commands.emplace_back(
                    outputs::rtsched::TaskRespTime{tid, search->second, time - search->second});
                extra_budget_times.erase(tid);
        }
}

void new_arrival(grid& grid, double time, std::size_t tid)
{
        grid.commands.emplace_back(outputs::rtsched::TaskArrival{tid, time});
}

void new_deadline(grid& grid, double time, std::size_t tid)
{
        grid.commands.emplace_back(outputs::rtsched::TaskDeadline{tid, time});
}

void plot(grid& grid, const std::vector<std::pair<double, traces::trace>>& traces)
{
        std::map<std::size_t, std::pair<double, std::size_t>> execution_times;
        std::map<std::size_t, double> extra_budget_times;

        for (const auto& tra : traces) {
                const auto timestamp = tra.first;
                const auto event = tra.second;
                std::visit(
                    overloaded{
                        [&timestamp, &grid](traces::job_arrival evt) {
                                new_arrival(grid, timestamp, evt.id);
                        },
                        [&timestamp, &grid](traces::serv_postpone evt) {
                                new_deadline(grid, evt.new_deadline, evt.id);
                        },
                        [&timestamp, &grid](traces::serv_ready evt) {
                                new_deadline(grid, evt.deadline, evt.id);
                        },
                        [&execution_times, &timestamp](traces::task_scheduled evt) {
                                open_execution_zone(
                                    execution_times, timestamp, evt.id, evt.proc_id);
                        },
                        [&execution_times, &timestamp, &grid](traces::task_preempted evt) {
                                close_execution_zone(execution_times, timestamp, evt.id, grid);
                        },
                        [&execution_times, &extra_budget_times, &timestamp,
                         &grid](traces::serv_non_cont evt) {
                                close_execution_zone(execution_times, timestamp, evt.id, grid);
                                open_extra_budget_zone(extra_budget_times, timestamp, evt.id);
                        },
                        [&execution_times, &extra_budget_times, &timestamp,
                         &grid](traces::serv_inactive evt) {
                                close_execution_zone(execution_times, timestamp, evt.id, grid);
                                close_extra_budget_zone(
                                    extra_budget_times, timestamp, evt.id, grid);
                        },
                        [](auto) {}},
                    event);
        }
}

void serialize(std::ostream& out, const command& com)
{
        return std::visit(
            overloaded{
                [&out](const TaskArrival& com) {
                        out << "\\TaskArrival{" << com.index << "}{" << com.arrival << "}";
                },
                [&out](const TaskDeadline& com) {
                        out << "\\TaskDeadline{" << com.index << "}{" << com.deadline << "}";
                },
                [&out](const TaskExecution& com) {
                        out << "\\TaskExecution[color=" << get_color(com.cpu) << "]{" << com.index
                            << "}{" << com.start << "}{" << com.stop << "}";
                },
                [&out](const TaskEnd& com) {
                        out << "\\TaskEnd{" << com.index << "}{" << com.stop << "}";
                },
                [&out](const TaskRespTime& com) {
                        out << "\\TaskRespTime{" << com.index << "}{" << com.start << "}{"
                            << com.stop << "}";
                }},
            com);
}

}; // namespace

namespace outputs::rtsched {

void print(std::ostream& out, const input_data& in)
{
        constexpr auto ADDITIONNAL_TIME_AFTER_LAST_EVENT{1};

        struct rtsched::grid grid;

        grid.nb_axis = count_tasks(in);
        grid.duration = std::ceil(get_last_timestamp(in) + ADDITIONNAL_TIME_AFTER_LAST_EVENT);

        plot(grid, in);

        out << "\\begin{RTGrid}{" << grid.nb_axis << "}{" << grid.duration << "}\n";
        for (const auto& com : grid.commands) {
                serialize(out, com);
                out << '\n';
        }
        out << "\\end{RTGrid}\n";
}

}; // namespace outputs::rtsched
