#ifndef SVG_HPP
#define SVG_HPP

#include "gantt.hpp"
#include "traces.hpp"
#include <map>
#include <string>

namespace outputs::gantt::svg {
	auto draw(const outputs::gantt::gantt& chart) -> std::string;
};
#endif
