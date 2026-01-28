#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace schedsim::core {

PowerDomain::PowerDomain(std::size_t id, std::vector<CStateLevel> c_states)
    : id_(id)
    , c_states_(std::move(c_states)) {}

int PowerDomain::achieved_cstate_for_processor(const Processor& proc) const {
    int requested_level = proc.current_cstate_level();

    // C0 (active) is always achievable
    if (requested_level <= 0) {
        return 0;
    }

    // Find the CStateLevel entry for this level
    const CStateLevel* cstate = nullptr;
    for (const auto& cs : c_states_) {
        if (cs.level == requested_level) {
            cstate = &cs;
            break;
        }
    }

    // If level not found, fall back to C0
    if (!cstate) {
        return 0;
    }

    // For per-processor C-states, the processor can achieve its requested level
    if (cstate->scope == CStateScope::PerProcessor) {
        return requested_level;
    }

    // For domain-wide C-states, check if all processors request at least this level
    return compute_achieved_cstate();
}

Duration PowerDomain::wake_latency(int level) const {
    // C0 (active) has no wake latency
    if (level <= 0) {
        return Duration{0.0};
    }

    // Find the CStateLevel entry for this level
    for (const auto& cs : c_states_) {
        if (cs.level == level) {
            return cs.wake_latency;
        }
    }

    // Level not found
    return Duration{0.0};
}

Power PowerDomain::cstate_power(int level) const {
    // Find the CStateLevel entry for this level
    for (const auto& cs : c_states_) {
        if (cs.level == level) {
            return cs.power;
        }
    }

    // Level not found
    return Power{0.0};
}

int PowerDomain::compute_achieved_cstate() const {
    // Find the minimum requested C-state level among all sleeping processors
    // Active (non-sleeping) processors implicitly request C0
    int min_level = std::numeric_limits<int>::max();

    for (const Processor* proc : processors_) {
        int level = 0;
        if (proc->state() == ProcessorState::Sleep) {
            level = proc->current_cstate_level();
        }
        min_level = std::min(min_level, level);
    }

    // Check each domain-wide C-state level from highest to lowest
    // Return the highest level that all processors can achieve
    int achieved = 0;
    for (const auto& cs : c_states_) {
        if (cs.scope == CStateScope::DomainWide) {
            if (cs.level <= min_level) {
                achieved = std::max(achieved, cs.level);
            }
        }
    }

    return achieved;
}

} // namespace schedsim::core
