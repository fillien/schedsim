#ifndef OUTPUT_TEXTUAL_HPP
#define OUTPUT_TEXTUAL_HPP

#include "traces.hpp"
#include <iostream>
#include <map>
#include <vector>

namespace outputs::textual {
void print(std::ostream& out, const std::multimap<double, traces::trace>& in);
};

#endif
