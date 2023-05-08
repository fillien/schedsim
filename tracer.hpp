#ifndef TRACER_HPP
#define TRACER_HPP

#include "trace.hpp"

#include <functional>

class tracer {
      private:
        std::vector<trace> trace_store;

      public:
        void add_trace(const trace& new_trace);
        void clear();
        auto format(std::function<std::string(const trace&)> func_format) -> std::string;
};

#endif
