#ifndef TRACER_HPP
#define TRACER_HPP

#include "event.hpp"

#include <functional>

class tracer {
      private:
        std::vector<trace> trace_store;

      public:
        void add_trace(const trace& new_trace);
        void clear();
        auto format(std::function<std::string(const trace&)> func_format) -> std::string;
};

auto operator<<(std::ostream& out, const trace& trace_to_print) -> std::ostream&;
auto to_txt(const trace& trace) -> std::string;

#endif
