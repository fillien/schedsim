#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <simulator/event.hpp>
#include <simulator/scheduler.hpp>

#include <memory>
#include <optional>
#include <set>
#include <vector>

class Engine;

namespace allocators {

/**
 * @brief Base class coordinating task placement across clusters.
 *
 * @details An Allocator orchestrates a set of child schedulers (one per cluster),
 * handles simulator events, decides initial placement for new jobs, and triggers
 * rescheduling as needed. Concrete allocators implement
 * where_to_put_the_task() to choose a destination scheduler for a newly
 * released job when it has no eligible server yet (or must be migrated).
 */
class Allocator : public Entity {
      public:
        /**
         * @brief Constructor for the Allocator class.
         *
         * @param sim A weak pointer to the simulation engine.
         */
        explicit Allocator(const std::weak_ptr<Engine>& sim) : Entity(sim) {}
        virtual ~Allocator() = default;
        Allocator(const Allocator&) = delete;
        auto operator=(const Allocator&) -> Allocator& = delete;
        Allocator(Allocator&&) noexcept = delete;
        auto operator=(Allocator&&) noexcept -> Allocator& = delete;

        /**
         * @brief Registers a child scheduler (cluster) managed by this allocator.
         *
         * @details Binds the provided scheduler to its cluster, exposes the
         * scheduler to the cluster, and marks it for rescheduling so it can
         * react to the new configuration.
         *
         * @param clu Weak pointer to the cluster associated with the scheduler.
         * @param sched Shared pointer to the scheduler instance to register.
         */
        auto add_child_sched(
            const std::weak_ptr<Cluster>& clu, const std::shared_ptr<scheds::Scheduler>& sched)
            -> void;

        /**
         * @brief Handle and dispatch a batch of simulator events.
         *
         * @details Events are processed in a deterministic priority order
         * (e.g., job completions before arrivals). Job arrivals are either
         * handed to the current server if already ready/running, or forwarded
         * to where_to_put_the_task() to select a destination scheduler. Any
         * schedulers marked via call_resched() are asked to reschedule after
         * the batch is processed.
         *
         * @param evts Vector of events occurring at the current simulation time.
         */
        auto handle(std::vector<events::Event> evts) -> void;

        /**
         * @brief Mark a scheduler to be rescheduled after current handling.
         *
         * @param index Shared pointer to the scheduler to reschedule.
         */
        auto call_resched(const std::shared_ptr<scheds::Scheduler>& index) -> void
        {
                rescheds_.insert(index);
        }

      protected:
        /**
         * @brief Decide the destination scheduler for a newly arrived job.
         *
         * @details Implementations typically inspect cluster capabilities,
         * admission tests, and allocator-specific policies to select a target
         * scheduler. Returning std::nullopt indicates the job must be rejected
         * at this simulation instant.
         *
         * @param new_task Shared pointer to the task to be placed.
         * @return Selected scheduler, or std::nullopt if no placement is possible.
         */
        virtual auto where_to_put_the_task(const std::shared_ptr<Task>& new_task)
            -> std::optional<std::shared_ptr<scheds::Scheduler>> = 0;

        /**
         * @brief Access the schedulers managed by this allocator.
         *
         * @return Const reference to the list of child schedulers (one per cluster).
         */
        [[nodiscard]] auto schedulers() const
            -> const std::vector<std::shared_ptr<scheds::Scheduler>>&
        {
                return schedulers_;
        }

      private:
        /**
         * @brief Migrate a task between schedulers as part of placement.
         *
         * @param evt Job arrival event that triggers the migration.
         * @param receiver Destination scheduler that will receive the job.
         */
        static auto migrate_task(
            const events::JobArrival& evt, const std::shared_ptr<scheds::Scheduler>& receiver)
            -> void;

        /**
         * @brief Internal helper to run placement for a new job arrival.
         *
         * @tparam T Concrete event type containing a `task_of_job` and `job_duration`.
         * @param new_job Event wrapper for the arriving job.
         */
        auto need_to_place_task(const auto& new_job) -> void;

        /** Child schedulers managed by the allocator (typically one per cluster). */
        std::vector<std::shared_ptr<scheds::Scheduler>> schedulers_;
        /** Schedulers flagged for rescheduling at the end of event handling. */
        std::set<std::shared_ptr<scheds::Scheduler>> rescheds_;
};

} // namespace allocators

#endif // ALLOCATOR_HPP
