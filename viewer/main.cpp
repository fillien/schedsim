#include <cmath>
#include <cstddef>
#include <fstream>
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
#include "rtsched.hpp"
#include "trace.hpp"

template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto count_tasks(const std::multimap<double, traces::trace>& traces) -> size_t {
        using namespace traces;

        std::set<size_t> cpt;

        for (const auto& tra : traces) {
                if (std::holds_alternative<job_arrival>(tra.second)) {
                        cpt.insert(std::get<job_arrival>(tra.second).task_id);
                }
        }

        return cpt.size();
}

auto get_last_timestamp(const std::multimap<double, traces::trace>& traces) -> double {
        double last_timestamp{0};
        if (traces.rbegin() != traces.rend()) {
                last_timestamp = std::prev(traces.end())->first;
        }

        return last_timestamp;
}

void plot_arrivals(rtsched::grid& grid, const std::multimap<double, traces::trace>& traces) {
	for (const auto& tra : traces) {
                const auto& time = tra.first;
                if (std::holds_alternative<traces::job_arrival>(tra.second)) {
                        const auto& job = std::get<traces::job_arrival>(tra.second);
			grid.commands.emplace_back(rtsched::TaskArrival{job.task_id, time});
		}
	}
}

void plot_deadlines(rtsched::grid& grid, const std::multimap<double, traces::trace>& traces) {
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

void plot_executions(rtsched::grid& grid, const std::multimap<double, traces::trace>& traces) {
        for (const auto& tra : traces) {
                if (std::holds_alternative<traces::task_scheduled>(tra.second)) {
			const auto& task_start = std::get<traces::task_scheduled>(tra.second);
			std::cout << "task " << task_start.task_id << "; start " << tra.first << std::endl;
                        for (const auto& tra_second : traces) {
				std::cout << tra.first << std::endl;
                                if (std::holds_alternative<traces::task_preempted>(
                                        tra_second.second)) {
					const auto& task_end = std::get<traces::task_preempted>(tra_second.second);
                                        if (task_start.task_id == task_end.task_id &&
                                            tra.first < tra_second.first) {
						std::cout << "task " << task_start.task_id << "; stop " << tra_second.first << std::endl;
                                                grid.commands.emplace_back(rtsched::TaskExecution{
                                                    task_start.task_id, tra.first,
                                                    tra_second.first});
						break;
					}
                                } else if (std::holds_alternative<traces::serv_non_cont>(tra_second.second)) {
                                        const auto& task_end =
                                            std::get<traces::serv_non_cont>(tra_second.second);
                                        if (task_start.task_id == task_end.serv_id &&
                                            tra.first < tra_second.first) {
						std::cout << "task " << task_start.task_id << "; stop " << tra_second.first << std::endl;
                                                grid.commands.emplace_back(rtsched::TaskExecution{
                                                    task_start.task_id, tra.first,
                                                    tra_second.first});
						break;
					}
                                } else if (std::holds_alternative<traces::serv_inactive>(tra_second.second)) {
					const auto& task_end = std::get<traces::serv_inactive>(tra_second.second);
                                        if ((task_start.task_id == task_end.serv_id) &&
                                            (tra.first < tra_second.first)) {
						std::cout << "task " << task_start.task_id << "; stop " << tra_second.first << std::endl;
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

        std::multimap<double, traces::trace> input_traces;

        for (const auto& dev : parsed_input) {
                input_traces.insert({dev.at("time").get<double>(), parse_trace(dev)});
		std::cout << "[" << dev.at("time").get<double>() << "] " << dev.at("type").get<std::string>() << std::endl;
        }

        struct rtsched::grid grid;

        grid.nb_axis = count_tasks(input_traces);
        grid.duration = get_last_timestamp(input_traces);

        plot_arrivals(grid, input_traces);
	plot_deadlines(grid, input_traces);
	plot_executions(grid, input_traces);

        std::ofstream mydessin;
        mydessin.open("mydessin.tex");
        mydessin << rtsched::grid_print(grid) << std::endl;
        mydessin.close();

        return 0;
}
