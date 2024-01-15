#ifndef OUTPUTS_SVG_HPP
#define OUTPUTS_SVG_HPP

#include "gantt.hpp"
#include <string>

namespace outputs::gantt::svg {
auto draw(const outputs::gantt::gantt& chart) -> std::string;
};

namespace outputs::gantt::html {
auto draw(const outputs::gantt::gantt& chart) -> std::string;
};
#endif
