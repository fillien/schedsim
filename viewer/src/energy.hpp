#ifndef ENERGY_HPP
#define ENERGY_HPP

#include "traces.hpp"

namespace outputs::energy {
void plot(const std::multimap<double, traces::trace>& input);
}
#endif
