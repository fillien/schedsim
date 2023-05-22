#ifndef TRACER_HPP
#define TRACER_HPP

#include "barectf.h"
#include "event.hpp"

#include <cstdint>
#include <functional>
#include <sys/types.h>

class tracer {
      private:
        std::vector<trace> trace_store;

        struct barectf_platform_simulator_ctx* platform_ctx;
        struct barectf_default_ctx* ctx;

      public:
        explicit tracer(double* clock);
        ~tracer();
        void add_trace(const trace& new_trace);
        void clear();
        auto format(std::function<std::string(const trace&)> func_format) -> std::string;
};

auto operator<<(std::ostream& out, const trace& trace_to_print) -> std::ostream&;
auto to_txt(const trace& trace) -> std::string;

#endif
