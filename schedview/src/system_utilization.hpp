#ifndef ACTIVE_UTIL_HPP
#define ACTIVE_UTIL_HPP

#include <map>
#include <protocols/traces.hpp>

namespace outputs::sys_util {
void print_active_utilization(const std::multimap<double, protocols::traces::trace>& input);
}

#endif
