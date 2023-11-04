#ifndef OUTPUT_TEXTUAL_HPP
#define OUTPUT_TEXTUAL_HPP

#include "../trace.hpp"
#include <iostream>
#include <vector>

namespace outputs::textual {
void print(std::ostream& out, const std::vector<std::pair<double, traces::trace>>& in);
};

#endif
