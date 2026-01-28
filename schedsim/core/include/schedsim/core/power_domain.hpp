#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace schedsim::core {

class Processor;

// C-state scope determines whether a C-state is per-processor or domain-wide
enum class CStateScope {
    PerProcessor,  // Each processor can enter/exit independently
    DomainWide     // All processors in domain must coordinate
};

// A single C-state level with its characteristics
struct CStateLevel {
    int level;               // C-state number (0 = active, higher = deeper sleep)
    CStateScope scope;       // Scope of this C-state
    Duration wake_latency;   // Time to wake from this C-state
    Power power;             // Power consumption in this state
};

// PowerDomain groups processors that share C-state management.
// All processors in a power domain share the same C-state levels.
class PowerDomain {
public:
    PowerDomain(std::size_t id, std::vector<CStateLevel> c_states);

    [[nodiscard]] std::size_t id() const noexcept { return id_; }
    [[nodiscard]] std::span<const CStateLevel> c_states() const noexcept { return c_states_; }
    [[nodiscard]] std::span<Processor* const> processors() const noexcept { return processors_; }

    // Get the achieved C-state level for a processor (handles domain-wide negotiation)
    // Returns the minimum of the requested level and what can be achieved given other processors
    [[nodiscard]] int achieved_cstate_for_processor(const Processor& proc) const;

    // Get wake-up latency for a C-state level
    // Returns zero duration if level is 0 (C0 = active) or level not found
    [[nodiscard]] Duration wake_latency(int level) const;

    // Get power consumption for a C-state level
    // Returns zero power if level not found
    [[nodiscard]] Power cstate_power(int level) const;

    // Non-copyable, movable (Decision 61)
    PowerDomain(const PowerDomain&) = delete;
    PowerDomain& operator=(const PowerDomain&) = delete;
    PowerDomain(PowerDomain&&) = default;
    PowerDomain& operator=(PowerDomain&&) = default;

private:
    friend class Platform;

    void add_processor(Processor* proc) { processors_.push_back(proc); }
    [[nodiscard]] int compute_achieved_cstate() const;

    std::size_t id_;
    std::vector<CStateLevel> c_states_;
    std::vector<Processor*> processors_;
};

} // namespace schedsim::core
