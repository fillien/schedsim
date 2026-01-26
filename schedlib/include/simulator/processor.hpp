#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <simulator/entity.hpp>
#include <simulator/timer.hpp>

#include <cstddef>
#include <memory>

class Cluster;
class Task;

/**
 * @brief Represents a processor model with a state and a running task.
 */
class Processor : public Entity {
      public:
        static constexpr double DPM_DELAY = 0.5;

        enum class State { Sleep, Idle, Running, Change };

        /**
         * @brief Constructs a Processor.
         * @param sim Reference to the engine.
         * @param cluster Pointer to the cluster.
         * @param cpu_id Unique ID of the processor.
         */
        explicit Processor(Engine& sim, Cluster* cluster, std::size_t cpu_id);

        [[nodiscard]] auto cluster() const -> Cluster* { return cluster_; }

        void task(Task* task_to_execute);
        void clear_task();
        [[nodiscard]] auto task() const -> Task* { return task_; }
        [[nodiscard]] auto has_task() const -> bool { return task_ != nullptr; }

        void update_state() { change_state(has_task() ? State::Running : State::Idle); }
        [[nodiscard]] auto state() const -> State { return current_state_; }
        [[nodiscard]] auto id() const -> std::size_t { return id_; }

        void change_state(const State& next_state);
        void dpm_change_state(const State& next_state);
        void dvfs_change_state(double delay);

      private:
        std::size_t id_;
        State current_state_{State::Idle};
        State dpm_target_{State::Idle};

        Task* task_{nullptr};
        Cluster* cluster_;
        std::unique_ptr<Timer> core_timer_;
};

#endif // PROCESSOR_HPP
