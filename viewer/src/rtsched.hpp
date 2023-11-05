#ifndef OUTPUTS_RTSCHED_HPP
#define OUTPUTS_RTSCHED_HPP

#include "trace.hpp"
#include <string>
#include <variant>
#include <vector>

namespace {
using input_data = std::vector<std::pair<double, traces::trace>>;
}

namespace outputs::rtsched {

struct TaskArrival {
        std::size_t index;
        double arrival;
};

struct TaskDeadline {
        std::size_t index;
        double deadline;
};

struct TaskExecution {
        std::size_t index;
        double start;
        double stop;
        std::size_t cpu;
};

struct TaskEnd {
        std::size_t index;
        double stop;
};

struct TaskRespTime {
        std::size_t index;
        double start;
        double stop;
};

using command = std::variant<TaskArrival, TaskDeadline, TaskExecution, TaskEnd, TaskRespTime>;

struct grid {
        std::size_t nb_axis;
        double duration;
        std::vector<command> commands;
};

void print(std::ostream& out, const input_data& in);

}; // namespace outputs::rtsched

#endif
