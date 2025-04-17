#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <simulator/entity.hpp>
#include <simulator/task.hpp>
#include <simulator/timer.hpp>

#include <memory>

class Cluster;

/**
 * @brief Represents a processor model with a state and a running task.
 */
class Processor : public Entity, public std::enable_shared_from_this<Processor> {
      public:
        static constexpr double DPM_DELAY = 0.5;

        /**
         * @brief Possible processor states.
         */
        enum class State { Sleep, Idle, Running, Change };

        /**
         * @brief Constructs a Processor.
         * @param sim Weak pointer to the engine.
         * @param cluster Weak pointer to the cluster.
         * @param cpu_id Unique ID of the processor.
         */
        explicit Processor(
            std::weak_ptr<Engine> sim, std::weak_ptr<Cluster> cluster, std::size_t cpu_id);

        /**
         * @brief Gets the cluster associated with the processor.
         * @return Shared pointer to the cluster.
         */
        [[nodiscard]] auto cluster() const -> std::shared_ptr<Cluster> { return cluster_.lock(); }

        /**
         * @brief Assigns a task to the processor.
         * @param task_to_execute Weak pointer to the task.
         */
        void task(std::weak_ptr<Task> task_to_execute);

        /**
         * @brief Clears the current task.
         */
        void clear_task();

        /**
         * @brief Retrieves the current task.
         * @return Shared pointer to the current task.
         */
        [[nodiscard]] auto task() const -> std::shared_ptr<Task>;

        /**
         * @brief Checks if the processor has an assigned task.
         * @return True if a task is assigned, false otherwise.
         */
        [[nodiscard]] auto has_task() const -> bool { return !task_.expired(); }

        /**
         * @brief Updates the processor state based on current activities.
         */
        void update_state() { change_state(has_task() ? State::Running : State::Idle); }

        /**
         * @brief Retrieves the current processor state.
         * @return Current processor state.
         */
        [[nodiscard]] auto state() const -> State { return current_state_; }

        /**
         * @brief Retrieves the processor's unique ID.
         * @return Processor's unique ID.
         */
        [[nodiscard]] auto id() const -> std::size_t { return id_; }

        /**
         * @brief Changes the processor state.
         * @param next_state The state to transition to.
         */
        void change_state(const State& next_state);

        /**
         * @brief Performs a DPM state transition.
         * @param next_state The state to transition to.
         */
        void dpm_change_state(const State& next_state);

        /**
         * @brief Performs a DVFS state transition with a delay.
         * @param delay The delay in seconds.
         */
        void dvfs_change_state(double delay);

      private:
        std::size_t id_;                   ///< Unique ID of the processor.
        State current_state_{State::Idle}; ///< Current processor state.
        State dpm_target_{State::Idle};    ///< Target state for DPM.

        std::weak_ptr<Task> task_;          ///< Currently running task.
        std::weak_ptr<Cluster> cluster_;    ///< Cluster containing this processor.
        std::shared_ptr<Timer> core_timer_; ///< Timer for processor operations.
};

#endif // PROCESSOR_HPP
