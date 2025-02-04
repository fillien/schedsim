#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include <entity.hpp>
#include <memory>
#include <task.hpp>
#include <timer.hpp>

class server;
class cluster;

/**
 * @brief Represents a processor model, composed of a state, and a running task.
 */
class processor : public entity, public std::enable_shared_from_this<processor> {
      public:
        static constexpr double DPM_DELAY{0.5};

        /**
         * @brief Possible states of a processor.
         */
        enum class state { sleep, idle, running, change };

        /**
         * @brief Class constructor.
         * @param sim Weak pointer to the engine.
         * @param cpu_id The unique ID of the processor.
         */
        explicit processor(
            const std::weak_ptr<engine>& sim,
            const std::weak_ptr<cluster>& clu,
            const std::size_t cpu_id);

        auto get_cluster() { return attached_cluster; };

        void set_task(const std::weak_ptr<task>& task_to_execute);

        void clear_task();

        auto get_task() const -> std::shared_ptr<task>
        {
                assert(!running_task.expired());
                return this->running_task.lock();
        };

        auto has_running_task() const -> bool { return !running_task.expired(); };

        /**
         * @brief Updates the state of the processor based on its current activities.
         */
        void update_state();

        /**
         * @brief Retrieves the current state of the processor.
         * @return Current state of the processor.
         */
        auto get_state() const -> state { return current_state; };

        /**
         * @brief Retrieves the unique ID of the processor.
         * @return Unique ID of the processor.
         */
        auto get_id() const -> std::size_t { return id; };

        /**
         * @brief Changes the state of the processor to the specified next state.
         * @param next_state The state to transition to.
         */
        void change_state(const state& next_state);

        void dpm_change_state(const state& next_state);

        void dvfs_change_state(const double& delay);

      private:
        /**
         * @brief Unique ID of the processor.
         */
        std::size_t id;

        /**
         * @brief Next the DPM will have to be in
         */
        state dpm_target;

        /**
         * @brief Weak pointer to the task currently running on the processor.
         */
        std::weak_ptr<task> running_task;

        std::weak_ptr<cluster> attached_cluster;

        /**
         * @brief Current state of the processor, initialized as idle by default.
         */
        state current_state{state::idle};

        std::shared_ptr<timer> coretimer;
};

#endif // PROCESSOR_HPP
