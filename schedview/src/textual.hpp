#ifndef OUTPUT_TEXTUAL_HPP
#define OUTPUT_TEXTUAL_HPP

#include <iostream>
#include <map>
#include <protocols/traces.hpp>
#include <vector>

namespace outputs::textual {
void print(std::ostream& out, const std::multimap<double, protocols::traces::trace>& in);
};

#endif
