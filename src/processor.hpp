#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include "entity.hpp"
#include <memory>
#include <vector>

class server;
class task;

/**
 * @brief A processor model, composed of a state, a running task and the rest of task ready to be
 * executed
 */
class processor : public entity {
      public:
        /**
         * @brief Possible states of a processor
         */
        enum class state { idle, running };

        /**
         * Unique id
         */
        int id;

        /**
         * @brief Current state of the processor, by default it's idle
         */
        state current_state{state::idle};

        /**
         * @brief Running queue of the active tasks
         */
        std::vector<std::weak_ptr<task>> runqueue{};

        /**
         * @brief The task currently executed on the processor, by default null because nothing is
         * executed.
         */
        std::shared_ptr<task> running_task{nullptr};

        /**
         * @brief Class constructor
         * @param id The unique id of the processor.
         */
        explicit processor(const std::weak_ptr<engine> sim, const int id);
};

#endif
