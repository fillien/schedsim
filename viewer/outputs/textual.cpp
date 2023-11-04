#include "textual.hpp"
#include "../rang.hpp"
#include <iomanip>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void outputs::textual::print(
    std::ostream& out, const std::vector<std::pair<double, traces::trace>>& in)
{
        constexpr auto TIME_LENGTH = 8;
        constexpr auto TIME_PRECISION = 5;

        constexpr auto color_name = [](std::ostream& out, const std::string& name) {
                constexpr int MAX_TRACE_NAME_SIZE = 23;
                out << rang::fg::magenta << rang::style::bold << std::setw(MAX_TRACE_NAME_SIZE)
                    << name << rang::style::reset << ": ";
        };

        constexpr auto color_arg = [](std::ostream& out, const std::string& name, const auto& arg) {
                out << rang::fg::cyan << name << rang::fg::reset << " = " << arg;
        };

        double last_time{0};
        for (const auto& tra : in) {
                std::cout << '[' << rang::fg::yellow << rang::style::bold << std::setw(TIME_LENGTH)
                          << std::setprecision(TIME_PRECISION) << std::fixed << tra.first
                          << std::defaultfloat << rang::style::reset << "] ";
                if (last_time < tra.first) {
                        std::cout << "(+" << std::setw(TIME_LENGTH)
                                  << std::setprecision(TIME_PRECISION) << std::fixed
                                  << (tra.first - last_time) << std::defaultfloat << ") ";
                }
                else {
                        std::cout << "( " << std::setw(TIME_LENGTH + 2) << ") ";
                }
                last_time = tra.first;
                std::visit(
                    overloaded{
                        [&out, color_name, color_arg](traces::job_arrival tra) {
                                color_name(out, "job_arrival");
                                color_arg(out, "tid", tra.id);
                                out << ", ";
                                color_arg(out, "duration", tra.job_duration);
                        },
                        [&out, color_name, color_arg](traces::job_finished tra) {
                                color_name(out, "job_finished");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::proc_activated tra) {
                                color_name(out, "proc_activated");
                                color_arg(out, "cpu", tra.id);
                        },
                        [&out, color_name, color_arg](traces::proc_idled tra) {
                                color_name(out, "proc_idled");
                                color_arg(out, "cpu", tra.id);
                        },
                        [&out, color_name, color_arg](traces::serv_budget_replenished tra) {
                                color_name(out, "serv_budget_replenished");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::serv_inactive tra) {
                                color_name(out, "serv_inactive");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::serv_budget_exhausted tra) {
                                color_name(out, "serv_budget_exhausted");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::serv_non_cont tra) {
                                color_name(out, "serv_non_cont");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::serv_postpone tra) {
                                color_name(out, "serv_postpone");
                                color_arg(out, "tid", tra.id);
                                out << ", ";
                                color_arg(out, "deadline", tra.new_deadline);
                        },
                        [&out, color_name, color_arg](traces::serv_ready tra) {
                                color_name(out, "serv_ready");
                                color_arg(out, "tid", tra.id);
                                out << ", ";
                                color_arg(out, "deadline", tra.deadline);
                        },
                        [&out, color_name, color_arg](traces::serv_running tra) {
                                color_name(out, "serv_running");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::task_preempted tra) {
                                color_name(out, "task_preempted");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::task_scheduled tra) {
                                color_name(out, "task_scheduled");
                                color_arg(out, "tid", tra.id);
                                out << ", ";
                                color_arg(out, "cpu", tra.proc_id);
                        },
                        [&out, color_name, color_arg](traces::task_rejected tra) {
                                color_name(out, "task_rejected");
                                color_arg(out, "tid", tra.id);
                        },
                        [&out, color_name, color_arg](traces::virtual_time_update tra) {
                                color_name(out, "virtual_time_update");
                                color_arg(out, "tid", tra.id);
                                out << ", ";
                                color_arg(out, "virtual_time", tra.new_virtual_time);
                        },
                        [&out, color_name, color_arg]([[maybe_unused]] traces::resched tra) {
                                color_name(out, "resched");
                        },
                        [&out, color_name, color_arg]([[maybe_unused]] traces::sim_finished tra) {
                                color_name(out, "sim_finished");
                        },
                        [&out, color_name, color_arg]([[maybe_unused]] auto& tra) {},
                    },
                    tra.second);
                std::cout << std::endl;
        }
}
