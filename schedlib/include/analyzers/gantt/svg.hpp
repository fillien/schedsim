#ifndef OUTPUTS_SVG_HPP
#define OUTPUTS_SVG_HPP

#include "analyzers/gantt/gantt.hpp"
#include <string>

namespace outputs::gantt::svg {
auto draw(const outputs::gantt::Gantt& chart) -> std::string;
};

namespace outputs::gantt::html {
auto draw(const outputs::gantt::Gantt& chart) -> std::string;
};
#endif
