#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP

#include "entity.hpp"
#include <memory>
#include <vector>

class server;
class task;

class processor : public entity {
      public:
        /// Possible states of a processor
        enum class state { idle, running, stopping, stopped };

        /// Unique id
        int id;

        /// Current state of the processor, by default it's idle
        state current_state{state::idle};

        /// Running queue of the active tasks
        std::vector<std::weak_ptr<task>> runqueue{};

        /// The task currently executed on the processor, by default null because nothing is
        /// executed
        std::shared_ptr<task> running_task{nullptr};

        processor(const int id);
};

#endif
