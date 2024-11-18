#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include "entity.hpp"
#include "timer.hpp"
#include <memory>

class server;

/**
 * @brief Represents a processor model, composed of a state, and a running task.
 */
class processor : public entity, public std::enable_shared_from_this<processor> {
      public:
        /**
         * @brief Possible states of a processor.
         */
        enum class state { sleep, idle, running, change };

        std::shared_ptr<timer> coretimer;

        /**
         * @brief Class constructor.
         * @param sim Weak pointer to the engine.
         * @param cpu_id The unique ID of the processor.
         */
        explicit processor(const std::weak_ptr<engine>& sim, std::size_t cpu_id);

        /**
         * @brief Sets the server to be executed on the processor.
         * @param server_to_execute Weak pointer to the server.
         */
        void set_server(std::weak_ptr<server> server_to_execute);

        /**
         * @brief Clears the server currently associated with the processor.
         */
        void clear_server();

        /**
         * @brief Retrieves the server currently associated with the processor.
         * @return Shared pointer to the server.
         */
        auto get_server() const -> std::shared_ptr<server>
        {
                assert(!running_server.expired());
                return this->running_server.lock();
        };

        /**
         * @brief Checks if a server is currently running on the processor.
         * @return True if a server is running, false otherwise.
         */
        auto has_server_running() const -> bool { return !running_server.expired(); };

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

      private:
        /**
         * @brief Unique ID of the processor.
         */
        std::size_t id;

        /**
         * @brief Weak pointer to the server currently running on the processor.
         */
        std::weak_ptr<server> running_server;

        /**
         * @brief Current state of the processor, initialized as idle by default.
         */
        state current_state{state::idle};
};

#endif // PROCESSOR_HPP
