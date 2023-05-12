#include "tracer.hpp"
#include "barectf-platform-simulator.h"
#include "barectf.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <sstream>

tracer::tracer(double* clock) {
        platform_ctx = barectf_platform_simulator_init(512, "trace/stream", 0, 0, 0, clock);
        ctx = barectf_platform_simulator_get_barectf_ctx(platform_ctx);
}

tracer::~tracer() { barectf_platform_simulator_fini(platform_ctx); }

void tracer::add_trace(const trace& new_trace) {
        using enum types;

        switch (new_trace.type) {
        case JOB_ARRIVAL: barectf_trace_job_arrival(ctx, new_trace.target_id); break;
        case JOB_FINISHED: barectf_trace_job_finished(ctx, new_trace.target_id); break;
        case PROC_IDLED: barectf_trace_proc_idle(ctx, ""); break;
        case RESCHED: barectf_trace_resched(ctx, ""); break;
        case SERV_ACT_CONT: barectf_trace_serv_act_cont(ctx, new_trace.target_id); break;
        case SERV_ACT_NON_CONT: barectf_trace_serv_act_non_cont(ctx, new_trace.target_id); break;
        case SERV_BUDGET_EXHAUSTED:
                barectf_trace_serv_budget_exhausted(ctx, new_trace.target_id);
                break;
        case SERV_BUDGET_REPLENISHED:
                barectf_trace_serv_budget_replenished(ctx, new_trace.target_id, new_trace.payload);
                break;
        case SERV_IDLE: barectf_trace_serv_idle(ctx, new_trace.target_id); break;
        case SERV_POSTPONE: barectf_trace_serv_postpone(ctx, new_trace.target_id); break;
        case TASK_PREEMPTED: barectf_trace_serv_preempted(ctx, new_trace.target_id); break;
        case SERV_RUNNING: barectf_trace_serv_running(ctx, new_trace.target_id); break;
        case TASK_SCHEDULED: barectf_trace_serv_scheduled(ctx, new_trace.target_id); break;
        case VIRTUAL_TIME_UPDATE:
                barectf_trace_virtual_time(ctx, new_trace.target_id, new_trace.payload);
                break;
        default: break;
        }
        trace_store.push_back(std::move(new_trace));
}
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

        out << trace.target_id << " " << std::setw(24) << std::left << trace.type << trace.payload
            << '\n';
        return out.str();
}
