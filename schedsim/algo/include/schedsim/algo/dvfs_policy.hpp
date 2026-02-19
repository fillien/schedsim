#pragma once

/// @file dvfs_policy.hpp
/// @brief Abstract base class and helpers for DVFS frequency scaling policies.
/// @ingroup algo_policies

#include <schedsim/core/types.hpp>

#include <cstddef>
#include <functional>
#include <unordered_map>

namespace schedsim::core {
class ClockDomain;
class Engine;
class Processor;
} // namespace schedsim::core

namespace schedsim::algo {

class EdfScheduler;

/// @brief Abstract interface for DVFS (Dynamic Voltage and Frequency Scaling) policies.
///
/// DVFS policies adjust processor clock-domain frequency based on workload
/// characteristics such as active utilization and per-server maximum utilization.
/// Concrete implementations (PowerAwareDvfsPolicy, FfaPolicy, CsfPolicy) override
/// the three event hooks to compute and apply a target frequency.
///
/// @ingroup algo_policies
/// @see EdfScheduler, CooldownTimer, FfaPolicy, CsfPolicy
class DvfsPolicy {
public:
    virtual ~DvfsPolicy() = default;

    /// @brief Called when active utilization changes.
    ///
    /// Triggered after job arrival, completion, server attach/detach, or any
    /// event that modifies the aggregate utilization seen by the scheduler.
    ///
    /// @param scheduler The EDF scheduler owning the affected processors.
    /// @param domain    The clock domain whose utilization changed.
    virtual void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) = 0;

    /// @brief Called when a processor becomes idle (no runnable job).
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that just became idle.
    virtual void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) = 0;

    /// @brief Called when a processor becomes active (starts executing a job).
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that just became active.
    virtual void on_processor_active(EdfScheduler& scheduler, core::Processor& proc) = 0;

    /// @brief Return the cooldown period between successive frequency changes.
    ///
    /// A non-zero value prevents rapid oscillation by enforcing a minimum
    /// interval between consecutive frequency adjustments on the same domain.
    ///
    /// @return The cooldown duration. Default is zero (no cooldown).
    [[nodiscard]] virtual core::Duration cooldown_period() const { return core::duration_from_seconds(0.0); }

    /// @brief Callback type invoked after a frequency change is applied.
    using FrequencyChangedCallback = std::function<void(core::ClockDomain&)>;

    /// @brief Register a callback to be notified when frequency changes.
    ///
    /// The EdfScheduler sets this callback so it can update job completion
    /// timers whenever the DVFS policy changes the clock domain frequency.
    ///
    /// @param callback Function to invoke after each frequency change.
    void set_frequency_changed_callback(FrequencyChangedCallback callback) {
        on_frequency_changed_ = std::move(callback);
    }

protected:
    /// @brief Callback invoked after a frequency change is applied.
    FrequencyChangedCallback on_frequency_changed_;
};

/// @brief Per-domain cooldown timer to throttle frequency changes.
///
/// Tracks the earliest time at which the next frequency change is permitted
/// for a given clock domain, preventing rapid DVFS oscillation.
///
/// @ingroup algo_policies
/// @see DvfsPolicy, PowerAwareDvfsPolicy
class CooldownTimer {
public:
    /// @brief Construct a cooldown timer.
    ///
    /// @param engine   The simulation engine (used to query the current time).
    /// @param cooldown Duration of the cooldown period.
    CooldownTimer(core::Engine& engine, core::Duration cooldown);

    /// @brief Check whether a frequency change is currently permitted.
    ///
    /// @return `true` if the cooldown has expired (or was never started).
    [[nodiscard]] bool can_change() const;

    /// @brief Start (or restart) the cooldown period from the current time.
    void start_cooldown();

    /// @brief Check whether the timer is still within the cooldown window.
    ///
    /// @return `true` if a cooldown is active and has not yet expired.
    [[nodiscard]] bool in_cooldown() const;

private:
    core::Engine& engine_;
    core::Duration cooldown_period_;
    core::TimePoint cooldown_until_{};
};

/// @brief Power-aware DVFS policy using the PA frequency formula.
///
/// Computes the minimum feasible frequency as:
///   f_min = f_max * ((m-1) * U_max + U_total) / m
///
/// where m is the number of processors in the domain, U_max is the maximum
/// single-server utilization, and U_total is the aggregate active utilization.
///
/// @ingroup algo_policies
/// @see DvfsPolicy, CooldownTimer, dvfs_dpm::compute_freq_min
class PowerAwareDvfsPolicy : public DvfsPolicy {
public:
    /// @brief Construct a power-aware DVFS policy.
    ///
    /// @param engine   The simulation engine (for time queries and tracing).
    /// @param cooldown Minimum interval between consecutive frequency changes.
    explicit PowerAwareDvfsPolicy(core::Engine& engine,
                                   core::Duration cooldown = core::duration_from_seconds(0.0));

    /// @brief Recompute and apply the target frequency after a utilization change.
    ///
    /// @param scheduler The EDF scheduler providing utilization data.
    /// @param domain    The clock domain to adjust.
    void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) override;

    /// @brief No-op for the PA policy (frequency depends only on utilization).
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that became idle.
    void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) override;

    /// @brief No-op for the PA policy (frequency depends only on utilization).
    ///
    /// @param scheduler The EDF scheduler owning the processor.
    /// @param proc      The processor that became active.
    void on_processor_active(EdfScheduler& scheduler, core::Processor& proc) override;

    /// @brief Return the configured cooldown period.
    ///
    /// @return The cooldown duration passed at construction.
    [[nodiscard]] core::Duration cooldown_period() const override { return cooldown_; }

private:
    /// @brief Apply a frequency change with cooldown enforcement.
    ///
    /// @param domain The clock domain to adjust.
    /// @param target The desired frequency.
    void apply_frequency_change(core::ClockDomain& domain, core::Frequency target);

    /// @brief Retrieve or create the cooldown timer for a clock domain.
    ///
    /// @param domain The clock domain.
    /// @return Reference to the per-domain CooldownTimer.
    [[nodiscard]] CooldownTimer& get_cooldown_timer(core::ClockDomain& domain);

    core::Engine& engine_;
    core::Duration cooldown_;
    std::unordered_map<std::size_t, CooldownTimer> domain_cooldowns_;
};

} // namespace schedsim::algo
