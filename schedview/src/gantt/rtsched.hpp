#ifndef OUTPUTS_RTSCHED_HPP
#define OUTPUTS_RTSCHED_HPP

#include "gantt.hpp"
#include <string>

namespace outputs::gantt::rtsched {
auto draw(const outputs::gantt::gantt& chart) -> std::string;
};

#endif
