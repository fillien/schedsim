#include <algorithm>
#include <engine.hpp>
#include <event.hpp>
#include <memory>
#include <processor.hpp>
#include <protocols/traces.hpp>
#include <ranges>
#include <schedulers/parallel.hpp>
#include <server.hpp>
#include <task.hpp>
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
        return deadline_order(*(first.task()->server()), *(second.task()->server()));
}

auto Parallel::get_inactive_bandwidth() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto NB_PROCS{static_cast<double>(get_nb_active_procs())};
        return NB_PROCS - ((NB_PROCS - 1) * u_max()) - total_utilization();
}

auto Parallel::get_nb_active_procs([[maybe_unused]] const double& new_utilization) const
    -> std::size_t
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        return chip()->processors().size();
}

auto Parallel::server_budget(const Server& serv) const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.utilization() / bandwidth * (serv.deadline() - serv.virtual_time());
}

auto Parallel::server_virtual_time(const Server& serv, const double& running_time) -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.virtual_time() + (bandwidth / serv.utilization() * running_time);
}

auto Parallel::admission_test(const Task& new_task) const -> bool
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto NB_PROCS{static_cast<double>(chip()->processors().size())};
        const auto U_MAX{std::max(u_max(), new_task.utilization())};
        const auto NEW_TOTAL_UTILIZATION{active_bandwidth() + new_task.utilization()};
        return (NEW_TOTAL_UTILIZATION <= (NB_PROCS - (NB_PROCS - 1) * U_MAX));
}

void Parallel::remove_task_from_cpu(const std::shared_ptr<Processor>& proc)
{
        if (proc->has_task()) {
                cancel_alarms(*(proc->task()->server()));

                proc->task()->server()->change_state(Server::State::Ready);
                proc->clear_task();
        }
}

void Parallel::on_resched()
{
        using std::ranges::empty;
        using std::ranges::max;
        using std::ranges::min;
        using std::views::filter;
        using enum Processor::State;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        update_running_servers();

        update_platform();

        // Place task using global EDF
        std::size_t cpt_scheduled_proc{0};

        const auto is_ready_server = [](const Server& serv) -> bool {
                return serv.state() == Server::State::Ready;
        };

        while (cpt_scheduled_proc < get_nb_active_procs()) {
                // refresh active servers list
                auto ready_servers = servers() | filter(from_shared<Server>(is_ready_server));
                auto available_procs =
                    chip()->processors() | filter([](const auto& proc) {
                            return proc->state() == Idle || proc->state() == Running;
                    });

                // Check if there are servers in ready or running state
                if (empty(ready_servers)) { break; }
                if (empty(available_procs)) { break; }

                // Get the server with the earliest deadline
                // Get the processeur that is idle or with the maximum deadline
                auto highest_priority_server =
                    min(ready_servers, from_shared<Server>(deadline_order));
                auto leastest_priority_processor =
                    min(available_procs, from_shared<Processor>(processor_order));

                if (leastest_priority_processor->state() == Sleep) {
                        assert(!leastest_priority_processor->has_task());
                }

                if ((!(leastest_priority_processor->state() == Change)) ||
                    !leastest_priority_processor->has_task() ||
                    deadline_order(
                        *highest_priority_server, *leastest_priority_processor->task()->server())) {

                        assert(leastest_priority_processor->state() != Sleep);
                        resched_proc(leastest_priority_processor, highest_priority_server);

                        cpt_scheduled_proc++;
                }
                else {
                        break;
                }
        }

        // Set next job finish or budget exhausted event for each proc with a task
        for (auto proc : chip()->processors()) {
                if (proc->state() == Sleep) { continue; }
                if (proc->state() == Change) { continue; }
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

void Parallel::update_platform() { chip()->freq(chip()->freq_max()); }

} // namespace scheds
