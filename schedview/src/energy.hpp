#ifndef ENERGY_HPP
#define ENERGY_HPP

#include <protocols/traces.hpp>

namespace outputs::energy {
void plot(const std::multimap<double, protocols::traces::trace>& input);
}
#endif
