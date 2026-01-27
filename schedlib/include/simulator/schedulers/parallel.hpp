#ifndef SCHED_PARALLEL_HPP
#define SCHED_PARALLEL_HPP

#include <simulator/engine.hpp>
#include <simulator/entity.hpp>
#include <simulator/processor.hpp>
#include <simulator/scheduler.hpp>

namespace scheds {

/**
 * @brief A parallel scheduler that implements custom scheduling logic
 *        for systems with parallel processing capabilities.
 */
class Parallel : public Scheduler {
      public:
        explicit Parallel(Engine& sim) : Scheduler(sim) {}

        static auto processor_order(const Processor& first, const Processor& second) -> bool;

        auto remove_task_from_cpu(Processor* proc) -> void;

        auto server_budget(const Server& serv) const -> double override;

        auto server_virtual_time(const Server& serv, const double& running_time) -> double override;

        auto compute_bandwidth() const -> double override;

        auto admission_test(const Task& new_task) const -> bool override;

        auto on_resched() -> void override;

        auto on_active_utilization_updated() -> void override {}

        auto update_platform() -> void override;

      protected:
        auto inactive_bandwidth() const -> double;

        auto nb_active_procs() const -> std::size_t;
};

} // namespace scheds

#endif // SCHED_PARALLEL_HPP
