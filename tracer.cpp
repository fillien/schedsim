#include "tracer.hpp"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <sstream>

void tracer::add_trace(const trace& new_trace) { trace_store.push_back(std::move(new_trace)); }
void tracer::clear() { trace_store.clear(); }

auto tracer::format(std::function<std::string(const trace&)> func_format) -> std::string {
        std::ostringstream out{};

        for (const auto& current_trace : trace_store) {
                out << func_format(current_trace);
        }
        return out.str();
}

auto to_txt(const trace& trace) -> std::string {
        std::ostringstream out{};

        out << "[t=" << std::setw(4) << trace.timestamp << "] " << std::setw(10) << std::left;

        using enum types;
        switch (trace.type) {
        case SERV_IDLE:
        case SERV_ACT_CONT:
        case SERV_RUNNING:
        case SERV_ACT_NON_CONT:
        case SERV_BUDGET_EXHAUSTED:
        case SERV_BUDGET_REPLENISHED: out << "Server "; break;

        case JOB_ARRIVAL:
        case JOB_FINISHED:
        case TASK_PREEMPTED:
        case TASK_SCHEDULED: out << "Task "; break;

        default: out << ""; break;
        }

        out << trace.target_id << " " << std::setw(24) << std::left << trace.type << trace.payload << '\n';
        return out.str();
}
