#include <analyzers/textual.hpp>
#include <protocols/traces.hpp>
#include <rang/rang.hpp>

#include <cstddef>
#include <iomanip>
#include <ostream>
#include <string>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace {
constexpr auto TIME_LENGTH = 11;
constexpr auto TIME_PRECISION = 5;

constexpr auto color_name = [](std::ostream& out, const std::string& name) {
        constexpr int MAX_TRACE_NAME_SIZE = 23;
        out << rang::fg::magenta << rang::style::bold << std::setw(MAX_TRACE_NAME_SIZE) << name
            << rang::style::reset << ": ";
};

constexpr auto color_arg = [](std::ostream& out, const std::string& name, const auto& arg) {
        out << rang::fg::cyan << name << rang::fg::reset << " = " << arg;
};

void serialize(std::ostream& out, const protocols::traces::trace& tra)
{
        namespace traces = protocols::traces;
        std::visit(
            overloaded{
                [&out](traces::JobArrival tra) {
                        color_name(out, "job_arrival");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "duration", tra.duration);
                        out << ", ";
                        color_arg(out, "deadline", tra.deadline);
                },
                [&out](traces::JobFinished tra) {
                        color_name(out, "job_finished");
                        color_arg(out, "tid", tra.task_id);
                },
                [&out](traces::ProcActivated tra) {
                        color_name(out, "proc_activated");
                        color_arg(out, "cluster_id", tra.cluster_id);
                        out << ", ";
                        color_arg(out, "cpu", tra.proc_id);
                },
                [&out](traces::ProcIdled tra) {
                        color_name(out, "proc_idled");
                        color_arg(out, "cluster_id", tra.cluster_id);
                        out << ", ";
                        color_arg(out, "cpu", tra.proc_id);
                },
                [&out](traces::ProcSleep tra) {
                        color_name(out, "proc_sleep");
                        color_arg(out, "cluster_id", tra.cluster_id);
                        out << ", ";
                        color_arg(out, "cpu", tra.proc_id);
                },
                [&out](traces::ProcChange tra) {
                        color_name(out, "proc_change");
                        color_arg(out, "cluster_id", tra.cluster_id);
                        out << ", ";
                        color_arg(out, "cpu", tra.proc_id);
                },
                [&out](traces::ServBudgetReplenished tra) {
                        color_name(out, "serv_budget_replenished");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "budget", tra.budget);
                },
                [&out](traces::ServInactive tra) {
                        color_name(out, "serv_inactive");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "utilization", tra.utilization);
                },
                [&out](traces::ServBudgetExhausted tra) {
                        color_name(out, "serv_budget_exhausted");
                        color_arg(out, "tid", tra.task_id);
                },
                [&out](traces::ServNonCont tra) {
                        color_name(out, "serv_non_cont");
                        color_arg(out, "tid", tra.task_id);
                },
                [&out](traces::ServPostpone tra) {
                        color_name(out, "serv_postpone");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "deadline", tra.deadline);
                },
                [&out](traces::ServReady tra) {
                        color_name(out, "serv_ready");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "deadline", tra.deadline);
                        out << ", ";
                        color_arg(out, "utilization", tra.utilization);
                },
                [&out](traces::ServRunning tra) {
                        color_name(out, "serv_running");
                        color_arg(out, "tid", tra.task_id);
                },
                [&out](traces::TaskPreempted tra) {
                        color_name(out, "task_preempted");
                        color_arg(out, "tid", tra.task_id);
                },
                [&out](traces::TaskScheduled tra) {
                        color_name(out, "task_scheduled");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "cpu", tra.proc_id);
                },
                [&out](traces::TaskRejected tra) {
                        color_name(out, "task_rejected");
                        color_arg(out, "tid", tra.task_id);
                },
                [&out](traces::VirtualTimeUpdate tra) {
                        color_name(out, "virtual_time_update");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "virtual_time", tra.virtual_time);
                },
                [&out](traces::FrequencyUpdate tra) {
                        color_name(out, "frequency_update");
                        color_arg(out, "cluster_id", tra.cluster_id);
                        out << ", ";
                        color_arg(out, "frequency", tra.frequency);
                },
                [&out]([[maybe_unused]] traces::Resched tra) { color_name(out, "resched"); },
                [&out]([[maybe_unused]] traces::SimFinished tra) {
                        color_name(out, "sim_finished");
                },
                [&out](traces::TaskPlaced tra) {
                        color_name(out, "task_placed");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "cluster_id", tra.cluster_id);
                },
                [&out](traces::MigrationCluster tra) {
                        color_name(out, "migration_cluster");
                        color_arg(out, "tid", tra.task_id);
                        out << ", ";
                        color_arg(out, "cluster_id", tra.cluster_id);
                },
                []([[maybe_unused]] auto&) {},
            },
            tra);
        out << '\n';
}

}; // namespace

void outputs::textual::print(
    std::ostream& out, const std::vector<std::pair<double, protocols::traces::trace>>& in)
{
        double last_timestamp{0};

        for (auto [timestamp, trace] : in) {
                out << '[' << rang::fg::yellow << rang::style::bold << std::setw(TIME_LENGTH)
                    << std::setprecision(TIME_PRECISION) << std::fixed << timestamp
                    << std::defaultfloat << rang::style::reset << "] ";
                if (last_timestamp < timestamp) {
                        out << "(+" << std::setw(TIME_LENGTH) << std::setprecision(TIME_PRECISION)
                            << std::fixed << (timestamp - last_timestamp) << std::defaultfloat
                            << ") ";
                }
                else {
                        out << "( " << std::setw(TIME_LENGTH + 2) << ") ";
                }
                last_timestamp = timestamp;
                serialize(out, trace);
        }
}
