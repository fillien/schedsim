#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include "entity.hpp"
#include <memory>
#include <vector>

class server;

/**
 * @brief A processor model, composed of a state, a running task and the rest of task ready to be
 * executed
 */
class processor : public entity, public std::enable_shared_from_this<processor> {
      public:
        /**
         * @brief Possible states of a processor
         */
        enum class state { idle, running };

        /**
         * @brief Class constructor
         * @param id The unique id of the processor.
         */
        explicit processor(const std::weak_ptr<engine>& sim, int cpu_id);

        void set_server(std::weak_ptr<server> server_to_execute);

        void clear_server();

        auto get_server() -> std::shared_ptr<server> {
                assert(!running_server.expired());
                return this->running_server.lock();
        };

        auto has_server_running() const -> bool { return !running_server.expired(); };
        void update_state();
        auto get_state() const { return current_state; };
        auto get_id() const { return id; };

      private:
        /**
         * Unique id
         */
        int id;

        /**
         * @brief The task currently running on the processor
         */
        std::weak_ptr<server> running_server;

        /**
         * @brief Current state of the processor, by default it's idle
         */
        state current_state{state::idle};

        void change_state(const state& next_state);
};

#endif
