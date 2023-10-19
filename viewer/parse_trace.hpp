#ifndef PARSE_TRACE
#define PARSE_TRACE

#include "../src/event.hpp"
#include "nlohmann/json.hpp"
#include "trace.hpp"

auto parse_trace(nlohmann::json trace) -> traces::trace;

#endif
