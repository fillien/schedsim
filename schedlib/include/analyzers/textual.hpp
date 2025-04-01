#ifndef OUTPUT_TEXTUAL_HPP
#define OUTPUT_TEXTUAL_HPP

#include <protocols/traces.hpp>

#include <iostream>
#include <vector>

namespace outputs::textual {
void serialize(std::ostream& out, const protocols::traces::trace& tra);
void print(std::ostream& out, const std::vector<std::pair<double, protocols::traces::trace>>& in);
}; // namespace outputs::textual

#endif
