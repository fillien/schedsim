#include "tracer.hpp"
#include "barectf-platform-simulator.h"
#include "barectf.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>

tracer::tracer(double* clock) {
        platform_ctx = barectf_platform_simulator_init(512, "trace/stream", 0, 0, 0, clock);
        ctx = barectf_platform_simulator_get_barectf_ctx(platform_ctx);
}

tracer::~tracer() { barectf_platform_simulator_fini(platform_ctx); }

void tracer::add_trace(const trace& new_trace) {
        using enum types;

        switch (new_trace.type) {
        case JOB_FINISHED: barectf_trace_job_finished(ctx, new_trace.target_id); break;
        case PROC_IDLED: barectf_trace_proc_idle(ctx); break;
        case PROC_ACTIVATED: barectf_trace_proc_activated(ctx); break;
        case RESCHED: barectf_trace_resched(ctx); break;
        case SERV_NON_CONT: barectf_trace_serv_non_cont(ctx, new_trace.target_id); break;
        case SERV_BUDGET_EXHAUSTED:
                barectf_trace_serv_budget_exhausted(ctx, new_trace.target_id);
                break;
        case SERV_BUDGET_REPLENISHED:
                barectf_trace_serv_budget_replenished(ctx, new_trace.target_id, new_trace.payload);
                break;
        case SERV_INACTIVE: barectf_trace_serv_inactive(ctx, new_trace.target_id); break;
        case SERV_POSTPONE: barectf_trace_serv_postpone(ctx, new_trace.target_id); break;
        case TASK_PREEMPTED: barectf_trace_serv_preempted(ctx, new_trace.target_id); break;
        case SERV_RUNNING: barectf_trace_serv_running(ctx, new_trace.target_id); break;
        case TASK_SCHEDULED: barectf_trace_serv_scheduled(ctx, new_trace.target_id); break;
        case VIRTUAL_TIME_UPDATE:
                barectf_trace_virtual_time(ctx, new_trace.target_id, new_trace.payload);
                break;
        case SIM_FINISHED: barectf_trace_sim_finished(ctx); break;
        default: break;
        }
        trace_store.push_back(std::move(new_trace));
}

void tracer::traceJobArrival(int serverId, int virtualTime, int deadline) {
        barectf_trace_job_arrival(ctx, serverId, virtualTime, deadline);
        std::cout << "Task " << serverId << " job arrival, virtual time = " << virtualTime
                  << ", deadline = " << deadline << '\n';
}

void tracer::traceGotoReady(int serverId) {
        barectf_trace_serv_ready(ctx, serverId);
        std::cout << "Server " << serverId << " go to ready state\n";
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

        out << "[t=" << trace.timestamp << "] " << std::setw(10) << std::left;

        using enum types;
        switch (trace.type) {
        case SERV_INACTIVE:
        case SERV_READY:
        case SERV_RUNNING:
        case SERV_NON_CONT:
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
