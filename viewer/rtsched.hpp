#include <cstddef>
#include <iostream>
#include <variant>
#include <vector>

namespace rtsched {
struct TaskArrival {
        size_t index;
        double arrival;
};

struct TaskDeadline {
        size_t index;
        double deadline;
};

struct TaskExecution {
        size_t index;
        double start;
        double stop;
};

struct TaskEnd {
        size_t index;
        double stop;
};

using command = std::variant<TaskArrival, TaskDeadline, TaskExecution, TaskEnd>;

struct grid {
        size_t nb_axis;
        double duration;
        std::vector<command> commands;
};

auto serialize(const command& com) -> std::string;
auto grid_print(const grid& grid) -> std::string;
} // namespace rtsched
