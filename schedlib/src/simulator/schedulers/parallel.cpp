#include <algorithm>
#include <memory>
#include <protocols/traces.hpp>
#include <ranges>
#include <simulator/engine.hpp>
#include <simulator/event.hpp>
#include <simulator/processor.hpp>
#include <simulator/schedulers/parallel.hpp>
#include <simulator/server.hpp>
#include <simulator/task.hpp>
#include <vector>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace scheds {

auto Parallel::processor_order(const Processor& first, const Processor& second) -> bool
{
        using enum Processor::State;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (!first.has_task()) { return (first.state() == Idle); }
        if (!second.has_task()) { return (second.state() == Sleep || second.state() == Change); }
        return deadline_order(*first.task()->server(), *second.task()->server());
}

auto Parallel::inactive_bandwidth() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto nb_procs = static_cast<double>(nb_active_procs());
        return nb_procs - ((nb_procs - 1) * u_max()) - total_utilization();
}

auto Parallel::nb_active_procs() const -> std::size_t { return chip()->processors().size(); }

auto Parallel::server_budget(const Server& serv) const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto nb_active_procs_val = static_cast<double>(nb_active_procs());
        const auto bandwidth = 1 - (inactive_bandwidth() / nb_active_procs_val);
        const auto scaled_utilization =
            (serv.utilization() * cluster()->scale_speed()) / cluster()->perf();
        return scaled_utilization / bandwidth * (serv.deadline() - serv.virtual_time());
}

auto Parallel::server_virtual_time(const Server& serv, const double& running_time) -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto nb_active_procs_val = static_cast<double>(nb_active_procs());
        const auto bandwidth = 1 - (inactive_bandwidth() / nb_active_procs_val);
        const auto scaled_utilization =
            (serv.utilization() * cluster()->scale_speed()) / cluster()->perf();
        return serv.virtual_time() + ((bandwidth / scaled_utilization) * running_time);
}

auto Parallel::admission_test(const Task& new_task) const -> bool
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto nb_procs = static_cast<double>(nb_active_procs());
        const auto scaled_utilization =
            (new_task.utilization() * cluster()->scale_speed()) / cluster()->perf();
        const auto u_max_val = std::max(u_max(), scaled_utilization);
        const auto new_total_util = active_bandwidth() + scaled_utilization;
        return (new_total_util <= (nb_procs - ((nb_procs - 1) * u_max_val)));
}

void Parallel::remove_task_from_cpu(const std::shared_ptr<Processor>& proc)
{
        if (proc->has_task()) {
                cancel_alarms(*proc->task()->server());
                proc->task()->server()->change_state(Server::State::Ready);
                proc->clear_task();
        }
}

void Parallel::on_resched()
{
        using std::ranges::empty;
        using std::ranges::min;
        using std::views::filter;
        using enum Processor::State;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        update_running_servers();
        update_platform();

        std::size_t num_scheduled_procs = 0;
        const std::size_t nb_procs = nb_active_procs();

        const auto is_ready_server = [](const Server& serv) -> bool {
                return serv.state() == Server::State::Ready;
        };

        // Schedule tasks using global EDF until all available processors are occupied.
        while (num_scheduled_procs < nb_procs) {
                auto ready_servers = servers() | filter(from_shared<Server>(is_ready_server));
                auto available_procs =
                    chip()->processors() | filter([](const auto& proc) {
                            return proc->state() == Idle || proc->state() == Running;
                    });

                if (empty(ready_servers) || empty(available_procs)) { break; }

                auto highest_priority_server =
                    min(ready_servers, from_shared<Server>(deadline_order));
                auto selected_proc = min(available_procs, from_shared<Processor>(processor_order));

                if (selected_proc->state() == Sleep) { assert(!selected_proc->has_task()); }

                const bool can_schedule =
                    (selected_proc->state() != Change) || (!selected_proc->has_task()) ||
                    deadline_order(*highest_priority_server, *selected_proc->task()->server());

                if (can_schedule) {
                        assert(selected_proc->state() != Sleep);
                        resched_proc(selected_proc, highest_priority_server);
                        ++num_scheduled_procs;
                }
                else {
                        break;
                }
        }

        // Update each processor: set next event or mark as idle.
        for (auto proc : chip()->processors()) {
                if (proc->state() == Sleep || proc->state() == Change) { continue; }
                if (proc->has_task()) {
                        cancel_alarms(*proc->task()->server());
                        activate_alarms(proc->task()->server());
                        proc->change_state(Running);
                }
                else {
                        proc->change_state(Idle);
                }
        }
}

void Parallel::update_platform() { chip()->dvfs_change_freq(chip()->freq_max()); }

} // namespace scheds
