#include "tracer.hpp"
#include "trace.hpp"

#include <fstream>
#include <functional>
#include <sstream>

void tracer::add_trace(const trace& new_trace) { trace_store.push_back(std::move(new_trace)); }
void tracer::clear() { trace_store.clear(); }

auto tracer::format(std::function<std::string(const trace&)> func_format) -> std::string {
        std::ostringstream out{};

        /// @todo Add a strategy pattern to handle CSV header, because it's impossible to do it with
        /// just a lambda.
        // output_file << "type,event,id,time,value" << std::endl;
        for (const auto& current_trace : trace_store) {
                out << func_format(current_trace);
        }
        return out.str();
}
