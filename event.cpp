#include "event.hpp"

#include <iostream>
#include <memory>
#include <vector>

event::event(const types type, const std::weak_ptr<entity> target, const double payload)
    : id(++cpt_id), type(type), target(target), payload(payload) {}

auto operator<<(std::ostream& out, const types& type) -> std::ostream& {
        using enum types;
        switch (type) {
        case JOB_ARRIVAL: return out << "JOB_ARRIVAL";
        case JOB_FINISHED: return out << "JOB_FINISHED";
        case PROC_ACTIVATED: return out << "PROC_ACTIVATED";
        case PROC_IDLED: return out << "PROC_IDLED";
        case RESCHED: return out << "RESCHED";
        case SERV_ACT_CONT: return out << "SERV_ACT_CONT";
        case SERV_ACT_NON_CONT: return out << "SERV_ACT_NON_CONT";
        case SERV_BUDGET_EXHAUSTED: return out << "SERV_BUDGET_EXHAUSTED";
        case SERV_BUDGET_REPLENISHED: return out << "SERV_BUDGET_REPLENISHED";
        case SERV_IDLE: return out << "SERV_IDLE";
        case SERV_RUNNING: return out << "SERV_RUNNING";
	case SERV_POSTPONE: return out << "SERV_POSTPONE";
        case SIM_FINISHED: return out << "SIM_FINISHED";
        case TASK_PREEMPTED: return out << "TASK_PREEMPTED";
        case TASK_SCHEDULED: return out << "TASK_SCHEDULED";
        default: return out << "unknown";
        }
}

auto operator<<(std::ostream& out, const event& ev) -> std::ostream& {
        return out << "[Event Id: " << ev.id << ", Type: " << ev.type << "]";
}
