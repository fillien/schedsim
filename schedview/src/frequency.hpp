#ifndef FREQUENCY_HPP
#define FREQUENCY_HPP

#include <protocols/traces.hpp>

namespace outputs::frequency {
void print_frequency_changes(const std::multimap<double, protocols::traces::trace>& input);
} // namespace outputs::frequency
#endif
