#pragma once

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace schedsim::core {

class Processor;

/// @brief Determines whether a C-state applies per-processor or domain-wide.
///
/// Shallow C-states are typically per-processor (each core can independently
/// enter or exit them), while deeper C-states may require all cores in a power
/// domain to coordinate.
enum class CStateScope {
    PerProcessor,  ///< Each processor can enter and exit this C-state independently.
    DomainWide     ///< All processors in the power domain must coordinate to enter this C-state.
};

/// @brief Describes a single C-state level and its hardware characteristics.
/// @ingroup core_hardware
///
/// C-states form an ordered hierarchy: level 0 is the fully active state (C0),
/// and increasing levels represent progressively deeper sleep states with lower
/// power consumption but higher wake-up latency.
///
/// @see PowerDomain, CStateScope
struct CStateLevel {
    int level;               ///< C-state number (0 = active/C0, higher = deeper sleep).
    CStateScope scope;       ///< Whether this C-state is per-processor or domain-wide.
    Duration wake_latency;   ///< Time required to transition from this C-state back to C0.
    Power power;             ///< Power consumption while in this C-state (mW).
};

/// @brief Groups processors that share C-state management.
/// @ingroup core_hardware
///
/// A PowerDomain models a shared power rail or power-gating region. All
/// processors in a power domain share the same set of available C-state
/// levels. For domain-wide C-states, all processors must agree before the
/// domain can enter a deeper sleep state.
///
/// PowerDomain is non-copyable but movable (Decision 61).
///
/// @see Processor, CStateLevel, CStateScope, ClockDomain
class PowerDomain {
public:
    /// @brief Construct a PowerDomain with its available C-state levels.
    /// @param id        Unique identifier within the platform.
    /// @param c_states  Ordered list of C-state levels supported by this domain.
    ///                  Level 0 (C0/active) should typically be included.
    PowerDomain(std::size_t id, std::vector<CStateLevel> c_states);

    /// @brief Return the unique identifier of this power domain.
    /// @return Power domain ID.
    [[nodiscard]] std::size_t id() const noexcept { return id_; }

    /// @brief Return the available C-state levels for this domain.
    /// @return A read-only span of CStateLevel descriptors.
    /// @see CStateLevel
    [[nodiscard]] std::span<const CStateLevel> c_states() const noexcept { return c_states_; }

    /// @brief Return the processors belonging to this power domain.
    /// @return A read-only span of processor pointers.
    /// @see Processor
    [[nodiscard]] std::span<Processor* const> processors() const noexcept { return processors_; }

    /// @brief Compute the effective C-state for a given processor.
    ///
    /// For per-processor C-states, this returns the processor's own requested
    /// level. For domain-wide C-states, the achieved level is the minimum of
    /// all processors' requested levels, since every processor must agree
    /// before the domain can enter a deeper state.
    ///
    /// @param proc  The processor to query.
    /// @return The effective C-state level that the processor can achieve.
    /// @see Processor::current_cstate_level, CStateScope
    [[nodiscard]] int achieved_cstate_for_processor(const Processor& proc) const;

    /// @brief Return the wake-up latency for a given C-state level.
    ///
    /// This is the time required to transition from the specified C-state
    /// back to the fully active state (C0).
    ///
    /// @param level  The C-state level to query.
    /// @return Wake-up latency duration. Returns a zero duration if @p level
    ///         is 0 (C0) or if the level is not found.
    /// @see CStateLevel::wake_latency
    [[nodiscard]] Duration wake_latency(int level) const;

    /// @brief Return the power consumption for a given C-state level.
    /// @param level  The C-state level to query.
    /// @return Power consumption (mW). Returns zero if @p level is not found.
    /// @see CStateLevel::power
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
