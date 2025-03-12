#ifndef OUTPUT_TEXTUAL_HPP
#define OUTPUT_TEXTUAL_HPP

#include <protocols/traces.hpp>

#include <iostream>
#include <vector>

namespace outputs::textual {
void print(std::ostream& out, const std::vector<std::pair<double, protocols::traces::trace>>& in);
};

#endif
