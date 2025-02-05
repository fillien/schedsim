#include <algorithm>
#include <engine.hpp>
#include <event.hpp>
#include <iterator>
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

auto parallel::processor_order(const processor& first, const processor& second) -> bool
{
        using enum processor::state;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (!first.has_running_task()) { return (first.get_state() == idle); }
        if (!second.has_running_task()) {
                return (second.get_state() == sleep || second.get_state() == change);
        }
        return deadline_order(
            *(first.get_task()->get_server()), *(second.get_task()->get_server()));
}

auto parallel::get_inactive_bandwidth() const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto TOTAL_UTILIZATION{get_total_utilization()};
        const auto MAX_UTILIZATION{get_max_utilization(servers)};
        const auto NB_PROCS{static_cast<double>(get_nb_active_procs())};
        return NB_PROCS - (NB_PROCS - 1) * MAX_UTILIZATION - TOTAL_UTILIZATION;
}

auto parallel::get_nb_active_procs([[maybe_unused]] const double& new_utilization) const
    -> std::size_t
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        return chip()->processors.size();
}

auto parallel::get_server_budget(const server& serv) const -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.utilization() / bandwidth * (serv.relative_deadline - serv.virtual_time);
}

auto parallel::get_server_virtual_time(const server& serv, const double& running_time) -> double
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const double NB_ACTIVE_PROCS{static_cast<double>(get_nb_active_procs())};
        const auto bandwidth{1 - (get_inactive_bandwidth() / NB_ACTIVE_PROCS)};
        return serv.virtual_time + bandwidth / serv.utilization() * running_time;
}

auto parallel::admission_test(const task& new_task) const -> bool
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        const auto NB_PROCS{static_cast<double>(chip()->processors.size())};
        const auto U_MAX{get_max_utilization(servers, new_task.utilization)};
        const auto NEW_TOTAL_UTILIZATION{get_total_utilization() + new_task.utilization};
        return (NEW_TOTAL_UTILIZATION <= (NB_PROCS - (NB_PROCS - 1) * U_MAX));
}

void parallel::remove_task_from_cpu(const std::shared_ptr<processor>& proc)
{
        if (proc->has_running_task()) {
                cancel_alarms(*(proc->get_task()->get_server()));

                proc->get_task()->get_server()->change_state(server::state::ready);
                proc->clear_task();
        }
}

void parallel::on_resched()
{
        using std::ranges::empty;
        using std::ranges::max;
        using std::ranges::min;
        using std::views::filter;
        using enum processor::state;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        update_running_servers();

        update_platform();

        // Place task using global EDF
        std::size_t cpt_scheduled_proc{0};

        while (cpt_scheduled_proc < get_nb_active_procs()) {
                // refresh active servers list
                auto ready_servers = servers | filter(from_shared<server>(is_ready_server));
                auto available_procs =
                    chip()->processors | filter([](const auto& proc) {
                            return proc->get_state() == idle || proc->get_state() == running;
                    });

                // Check if there are servers in ready or running state
                if (empty(ready_servers)) { break; }
                if (empty(available_procs)) { break; }

                // Get the server with the earliest deadline
                // Get the processeur that is idle or with the maximum deadline
                auto highest_priority_server =
                    min(ready_servers, from_shared<server>(deadline_order));
                auto leastest_priority_processor =
                    min(available_procs, from_shared<processor>(processor_order));

                if (leastest_priority_processor->get_state() == sleep) {
                        assert(!leastest_priority_processor->has_running_task());
                }

                if ((!(leastest_priority_processor->get_state() == change)) ||
                    !leastest_priority_processor->has_running_task() ||
                    deadline_order(
                        *highest_priority_server,
                        *leastest_priority_processor->get_task()->get_server())) {

                        assert(leastest_priority_processor->get_state() != sleep);
                        resched_proc(leastest_priority_processor, highest_priority_server);

                        cpt_scheduled_proc++;
                }
                else {
                        break;
                }
        }

        // Set next job finish or budget exhausted event for each proc with a task
        for (auto proc : chip()->processors) {
                if (proc->get_state() == sleep) { continue; }
                else if (proc->get_state() == change) {
                        continue;
                }
                else if (proc->has_running_task()) {
                        cancel_alarms(*proc->get_task()->get_server());
                        set_alarms(proc->get_task()->get_server());
                        proc->change_state(running);
                }
                else {
                        proc->change_state(idle);
                }
        }
}

void parallel::update_platform() { chip()->set_freq(chip()->freq_max()); }

} // namespace scheds
