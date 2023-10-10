#ifndef TRACER_HPP
#define TRACER_HPP

#include "barectf.h"
#include "event.hpp"

#include <cstdint>
#include <functional>
#include <sys/types.h>

/**
 * @brief A class that help to store logs and export them to Common Trace Format.
 */
class tracer {
      private:
        /**
         * @brief The vector that store the logs
         */
        std::vector<trace> trace_store;

        /**
         * @brief The barectf platform object pointer
         */
        struct barectf_platform_simulator_ctx* platform_ctx;

        /**
         * @brief The context of barectf who handle new event to export
         */
        struct barectf_default_ctx* ctx;

      public:
        /**
         * @brief Class constructor
         * @param clock A pointer to a variable that represent time and pass it to barectf
         */
        explicit tracer(double* clock);

        /**
         * @brief Class destructor, it release ressources of the barectf context
         */
        ~tracer();

        /**
         * @brief Add a new trace in logs
         * @param new_trace The new trace to add
         */
        void add_trace(const trace& new_trace);

        void traceJobArrival(int m_server_id, double m_virtual_time, double m_deadline);
        void traceGotoReady(int m_server_id);

        /**
         * @brief Delete the logs
         */
        void clear();

        /**
         * @brief A high order function who use a fonction that serialize a trace and pass to it all
         * the logs
         * @param func_format The fonction to use to serialize each logs entry
         */
        auto format(std::function<std::string(const trace&)> func_format) -> std::string;
};

/**
 * @brief Serialize a trace object to an output stream
 */
auto operator<<(std::ostream& out, const trace& trace_to_print) -> std::ostream&;

/**
 * @brief A function that serialize the trace in a readable format
 * @param trace The trace to serialize in string
 */
auto to_txt(const trace& trace) -> std::string;

#endif
