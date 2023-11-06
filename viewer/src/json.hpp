#ifndef INPUTS_JSON_HPP
#define INPUTS_JSON_HPP

#include "trace.hpp"
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace inputs::json {
void parse(const std::string& input_text, std::vector<std::pair<double, traces::trace>>& out);
auto parse_trace(nlohmann::json trace) -> traces::trace;
}; // namespace inputs::json
#endif
