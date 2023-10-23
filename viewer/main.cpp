#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <variant>
#include <vector>

#include "../src/event.hpp"
#include "nlohmann/json_fwd.hpp"
#include "parse_trace.hpp"
#include "rang.hpp"
#include "rtsched.hpp"
#include "trace.hpp"

template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto count_tasks(const std::vector<std::pair<double, traces::trace>>& traces) -> size_t {
        using namespace traces;

        std::set<size_t> cpt;

        for (const auto& tra : traces) {
                if (std::holds_alternative<job_arrival>(tra.second)) {
                        cpt.insert(std::get<job_arrival>(tra.second).task_id);
                }
        }

        return cpt.size();
}

auto get_last_timestamp(const std::vector<std::pair<double, traces::trace>>& traces) -> double {
        double last_timestamp{0};
        if (traces.rbegin() != traces.rend()) {
                last_timestamp = std::prev(traces.end())->first;
        }

        return last_timestamp;
}

void print_traces(const std::vector<std::pair<double, traces::trace>>& traces) {
        double last_time{0};
        for (const auto& tra : traces) {
                std::cout << '[' << rang::fg::yellow << rang::style::bold << std::setw(8)
                          << std::setprecision(5) << std::fixed << tra.first << std::defaultfloat
                          << rang::style::reset << "] ";
                if (last_time < tra.first) {
                        std::cout << "(+" << std::setw(8) << std::setprecision(5) << std::fixed
                                  << (tra.first - last_time) << std::defaultfloat << ") ";
                } else {
                        std::cout << "( " << std::setw(10) << ") ";
                }
                last_time = tra.first;
                std::visit(
                    overloaded{
                        [](traces::job_arrival tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "job_arrival" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.task_id << ", " << rang::fg::cyan << "duration"
                                          << rang::fg::reset << " = " << tra.job_duration;
                        },
                        [](traces::job_finished tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "job_finished" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.task_id;
                        },
                        [](traces::proc_activated tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "proc_activated" << rang::style::reset << ": "
                                          << rang::fg::cyan << "cpu" << rang::fg::reset << " = "
                                          << tra.proc_id;
                        },
                        [](traces::proc_idled tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "proc_idled" << rang::style::reset << ": "
                                          << rang::fg::cyan << "cpu" << rang::fg::reset << " = "
                                          << tra.proc_id;
                        },
                        [](traces::serv_budget_replenished tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "serv_budget_replenished" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.serv_id;
                        },
                        [](traces::serv_inactive tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "serv_inactive" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.serv_id;
                        },
                        [](traces::serv_budget_exhausted tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "serv_budget_exhausted" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.serv_id;
                        },
                        [](traces::serv_non_cont tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "serv_non_cont" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.serv_id;
                        },
                        [](traces::serv_postpone tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "serv_postpone" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.serv_id << ", " << rang::fg::cyan << "deadline"
                                          << rang::fg::reset << " = " << tra.new_deadline;
                        },
                        [](traces::serv_ready tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "serv_ready" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.serv_id << ", " << rang::fg::cyan << "deadline"
                                          << rang::fg::reset << " = " << tra.deadline;
                        },
                        [](traces::serv_running tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "serv_running" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.serv_id;
                        },
                        [](traces::task_preempted tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "task_preempted" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.task_id;
                        },
                        [](traces::task_scheduled tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "task_scheduled" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.task_id << ", " << rang::fg::cyan << "cpu"
                                          << rang::fg::reset << " = " << tra.proc_id;
                        },
                        [](traces::task_rejected tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "task_rejected" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.task_id;
                        },
                        [](traces::virtual_time_update tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "virtual_time_update" << rang::style::reset << ": "
                                          << rang::fg::cyan << "tid" << rang::fg::reset << " = "
                                          << tra.task_id << ", " << rang::fg::cyan << "virtual_time"
                                          << rang::fg::reset << " = " << tra.new_virtual_time;
                        },
                        [](traces::resched tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "resched" << rang::style::reset << ": ";
                        },
                        [](traces::sim_finished tra) {
                                std::cout << rang::fg::magenta << rang::style::bold << std::setw(23)
                                          << "sim_finished" << rang::style::reset << ": ";
                        },
                        [](auto& tra) { std::cout << "hello"; },
                    },
                    tra.second);
                std::cout << std::endl;
        }
}

void plot_arrivals(rtsched::grid& grid,
                   const std::vector<std::pair<double, traces::trace>>& traces) {
        for (const auto& tra : traces) {
                const auto& time = tra.first;
                if (std::holds_alternative<traces::job_arrival>(tra.second)) {
                        const auto& job = std::get<traces::job_arrival>(tra.second);
                        grid.commands.emplace_back(rtsched::TaskArrival{job.task_id, time});
                }
        }
}

void plot_deadlines(rtsched::grid& grid,
                    const std::vector<std::pair<double, traces::trace>>& traces) {
        for (const auto& tra : traces) {
                if (std::holds_alternative<traces::serv_postpone>(tra.second)) {
                        const auto& task = std::get<traces::serv_postpone>(tra.second);
                        if (task.new_deadline <= grid.duration) {
                                grid.commands.emplace_back(
                                    rtsched::TaskDeadline{task.serv_id, task.new_deadline});
                        }
                } else if (std::holds_alternative<traces::serv_ready>(tra.second)) {
                        const auto& task = std::get<traces::serv_ready>(tra.second);
                        if (task.deadline <= grid.duration) {
                                grid.commands.emplace_back(
                                    rtsched::TaskDeadline{task.serv_id, task.deadline});
                        }
                }
        }
}

void plot_executions(rtsched::grid& grid,
                     const std::vector<std::pair<double, traces::trace>>& traces) {
        for (const auto& tra : traces) {
                if (std::holds_alternative<traces::task_scheduled>(tra.second)) {
                        const auto& task_start = std::get<traces::task_scheduled>(tra.second);
                        std::cout << "task " << task_start.task_id << "; start " << tra.first
                                  << std::endl;
                        for (const auto& tra_second : traces) {
                                if (std::holds_alternative<traces::task_preempted>(
                                        tra_second.second)) {
                                        const auto& task_end =
                                            std::get<traces::task_preempted>(tra_second.second);
                                        if (task_start.task_id == task_end.task_id &&
                                            tra.first < tra_second.first) {
                                                std::cout << "task " << task_start.task_id
                                                          << "; stop " << tra_second.first
                                                          << std::endl;
                                                grid.commands.emplace_back(rtsched::TaskExecution{
                                                    task_start.task_id, tra.first,
                                                    tra_second.first});
                                                break;
                                        }
                                } else if (std::holds_alternative<traces::serv_non_cont>(
                                               tra_second.second)) {
                                        const auto& task_end =
                                            std::get<traces::serv_non_cont>(tra_second.second);
                                        if (task_start.task_id == task_end.serv_id &&
                                            tra.first < tra_second.first) {
                                                std::cout << "task " << task_start.task_id
                                                          << "; stop " << tra_second.first
                                                          << std::endl;
                                                grid.commands.emplace_back(rtsched::TaskExecution{
                                                    task_start.task_id, tra.first,
                                                    tra_second.first});
                                                break;
                                        }
                                } else if (std::holds_alternative<traces::serv_inactive>(
                                               tra_second.second)) {
                                        const auto& task_end =
                                            std::get<traces::serv_inactive>(tra_second.second);
                                        if ((task_start.task_id == task_end.serv_id) &&
                                            (tra.first < tra_second.first)) {
                                                std::cout << "task " << task_start.task_id
                                                          << "; stop " << tra_second.first
                                                          << std::endl;
                                                grid.commands.emplace_back(rtsched::TaskExecution{
                                                    task_start.task_id, tra.first,
                                                    tra_second.first});
                                                break;
                                        }
                                }
                        }
                }
        }
}

auto main() -> int {
        std::ifstream input_file("out.json");
        auto parsed_input = nlohmann::json::parse(input_file);

        std::vector<std::pair<double, traces::trace>> input_traces;

        for (const auto& dev : parsed_input) {
                input_traces.push_back({dev.at("time").get<double>(), parse_trace(dev)});
        }

        std::cout << "size: " << input_traces.size() << std::endl;

        print_traces(input_traces);

        struct rtsched::grid grid;

        grid.nb_axis = count_tasks(input_traces);
        grid.duration = get_last_timestamp(input_traces) + 1;

        plot_arrivals(grid, input_traces);
        plot_deadlines(grid, input_traces);
        plot_executions(grid, input_traces);

        std::ofstream mydessin;
        mydessin.open("mydessin.tex");
        mydessin << rtsched::grid_print(grid) << std::endl;
        mydessin.close();

        return 0;
}
