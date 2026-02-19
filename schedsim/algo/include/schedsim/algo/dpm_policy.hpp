#pragma once

/// @file dpm_policy.hpp
/// @brief Abstract base class and basic implementation for DPM power management policies.
/// @ingroup algo_policies

#include <schedsim/core/types.hpp>

#include <unordered_set>

namespace schedsim::core {
class Processor;
} // namespace schedsim::core

namespace schedsim::algo {

class EdfScheduler;

/// @brief Abstract interface for DPM (Dynamic Power Management) policies.
///
/// DPM policies manage processor sleep states (C-states) to reduce static
/// power consumption when cores are idle. The EdfScheduler invokes the two
/// hooks below at the appropriate scheduling points.
///
/// @ingroup algo_policies
/// @see BasicDpmPolicy, DvfsPolicy, EdfScheduler
class DpmPolicy {
public:
    virtual ~DpmPolicy() = default;

    /// @brief Called when a processor becomes idle.
    ///
    /// The policy should decide whether to put the processor into a low-power
    /// sleep state (C-state) or leave it active for faster wake-up.
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that just became idle.
    virtual void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) = 0;

    /// @brief Called when a processor is needed (jobs are waiting to be scheduled).
    ///
    /// The policy should wake up one or more sleeping processors so they can
    /// execute pending jobs.
    ///
    /// @param scheduler The EDF scheduler that needs a processor.
    virtual void on_processor_needed(EdfScheduler& scheduler) = 0;
};

/// @brief Basic DPM policy that puts idle processors to sleep immediately.
///
/// When a processor becomes idle, it is transitioned to the configured
/// C-state. When the scheduler signals that a processor is needed, one of
/// the sleeping processors is woken up.
///
/// @ingroup algo_policies
/// @see DpmPolicy, DvfsPolicy, EdfScheduler
class BasicDpmPolicy : public DpmPolicy {
public:
    /// @brief Construct a basic DPM policy.
    ///
    /// @param target_cstate   The C-state level to enter when idle
    ///                        (1 = C1, 2 = C2, etc.).
    /// @param idle_threshold  Minimum idle duration before entering sleep.
    ///                        Zero means immediate sleep on idle.
    explicit BasicDpmPolicy(int target_cstate = 1,
                            core::Duration idle_threshold = core::duration_from_seconds(0.0));

    /// @brief Put the processor to sleep if appropriate.
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that just became idle.
    void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) override;

    /// @brief Wake a sleeping processor to service pending jobs.
    ///
    /// @param scheduler The EDF scheduler that needs a processor.
    void on_processor_needed(EdfScheduler& scheduler) override;

    /// @brief Return the target C-state level.
    ///
    /// @return The C-state index (e.g., 1 for C1).
    [[nodiscard]] int target_cstate() const noexcept { return target_cstate_; }

    /// @brief Return the idle threshold duration.
    ///
    /// @return The minimum idle duration before the processor enters sleep.
    [[nodiscard]] core::Duration idle_threshold() const noexcept { return idle_threshold_; }

    /// @brief Return the number of processors currently sleeping.
    ///
    /// @return Count of processors managed by this policy that are in a sleep state.
    [[nodiscard]] std::size_t sleeping_processor_count() const noexcept {
        return sleeping_processors_.size();
    }

private:
    int target_cstate_;
    core::Duration idle_threshold_;
    std::unordered_set<core::Processor*> sleeping_processors_;
};

} // namespace schedsim::algo
