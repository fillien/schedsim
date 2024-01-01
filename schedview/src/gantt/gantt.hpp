#ifndef GANTT_HPP
#define GANTT_HPP

#include <vector>
#include <variant>
#include <map>
#include "traces.hpp"

namespace outputs::gantt {

struct arrival {
        std::size_t index;
        double timestamp;
};

struct deadline {
        std::size_t index;
        double timestamp;
};

struct execution {
        std::size_t index;
	std::size_t cpu;
        double start;
        double stop;
};

struct active_non_cont {
        std::size_t index;
        double start;
        double stop;
};

using command = std::variant<arrival, deadline, execution, active_non_cont>;

struct gantt {
        std::size_t nb_axis;
        double duration;
        std::vector<command> commands;
};

auto generate_gantt(const std::multimap<double, traces::trace>& logs) -> gantt;
};

#endif
