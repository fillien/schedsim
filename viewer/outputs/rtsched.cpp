#include "rtsched.hpp"

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

auto count_tasks(const std::vector<std::pair<double, traces::trace>>& traces) -> std::size_t
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

auto get_last_timestamp(const std::vector<std::pair<double, traces::trace>>& traces) -> double
{
        double last_timestamp{0};
        if (traces.rbegin() != traces.rend()) {
                last_timestamp = std::prev(traces.end())->first;
        }

        return last_timestamp;
}

void open_execution_zone(std::map<std::size_t, double>& start_times, double time, std::size_t tid)
{
        if (auto search = start_times.find(tid); search == std::end(start_times)) {
                start_times.insert({tid, time});
        }
}

void close_execution_zone(
    std::map<std::size_t, double>& start_times, double time, std::size_t tid, grid& grid)
{
        if (auto search = start_times.find(tid); search != std::end(start_times)) {
                grid.commands.emplace_back(
                    outputs::rtsched::TaskExecution{tid, search->second, time});
                start_times.erase(tid);
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

}; // namespace

namespace outputs::rtsched {

void plot(rtsched::grid& grid, const std::vector<std::pair<double, traces::trace>>& traces)
{
        std::map<std::size_t, double> start_times;

        for (const auto& tra : traces) {
                double time = tra.first;
                std::visit(
                    overloaded{
                        [&time, &grid](traces::job_arrival evt) {
                                new_arrival(grid, time, evt.id);
                        },
                        [&time, &grid](traces::serv_postpone evt) {
                                new_deadline(grid, time, evt.id);
                        },
                        [&time, &grid](traces::serv_ready evt) {
                                new_deadline(grid, time, evt.id);
                        },

                        // Openning execution zone event
                        [&start_times, &time](traces::task_scheduled evt) {
                                open_execution_zone(start_times, time, evt.id);
                        },
                        // Closing execution zone events
                        [&start_times, &time, &grid](traces::task_preempted evt) {
                                close_execution_zone(start_times, time, evt.id, grid);
                        },
                        [&start_times, &time, &grid](traces::serv_non_cont evt) {
                                close_execution_zone(start_times, time, evt.id, grid);
                        },
                        [&start_times, &time, &grid](traces::serv_inactive evt) {
                                close_execution_zone(start_times, time, evt.id, grid);
                        },
                        [](auto) {}},
                    tra.second);
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
                        out << "\\TaskExecution{" << com.index << "}{" << com.start << "}{"
                            << com.stop << "}";
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

void print(std::ostream& out, const std::vector<std::pair<double, traces::trace>>& in)
{
        constexpr auto ADDITIONNAL_TIME_AFTER_LAST_EVENT{1};

        struct rtsched::grid grid;

        grid.nb_axis = count_tasks(in);
        grid.duration = get_last_timestamp(in) + ADDITIONNAL_TIME_AFTER_LAST_EVENT;

        plot(grid, in);

        out << "\\begin{RTGrid}{" << grid.nb_axis << "}{" << grid.duration << "}\n";
        for (const auto& com : grid.commands) {
                serialize(out, com);
                out << '\n';
        }
        out << "\\end{RTGrid}\n";
}

}; // namespace outputs::rtsched
