#pragma once

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

// Interface for DVFS (Dynamic Voltage and Frequency Scaling) policies
// DVFS policies adjust processor frequency based on workload characteristics
class DvfsPolicy {
public:
    virtual ~DvfsPolicy() = default;

    // Called when active utilization changes (after job arrival, completion, etc.)
    virtual void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) = 0;

    // Called when a processor becomes idle
    virtual void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) = 0;

    // Called when a processor becomes active
    virtual void on_processor_active(EdfScheduler& scheduler, core::Processor& proc) = 0;

    // Cooldown period between frequency changes (0 = no cooldown)
    [[nodiscard]] virtual core::Duration cooldown_period() const { return core::Duration{0.0}; }

    // Callback registration for frequency changes
    // EdfScheduler sets this callback to be notified when frequency changes
    using FrequencyChangedCallback = std::function<void(core::ClockDomain&)>;
    void set_frequency_changed_callback(FrequencyChangedCallback callback) {
        on_frequency_changed_ = std::move(callback);
    }

protected:
    FrequencyChangedCallback on_frequency_changed_;
};

// Helper class for managing cooldown timers per clock domain
class CooldownTimer {
public:
    CooldownTimer(core::Engine& engine, core::Duration cooldown);

    // Check if a frequency change is allowed (cooldown has expired)
    [[nodiscard]] bool can_change() const;

    // Start the cooldown period
    void start_cooldown();

    // Check if currently in cooldown
    [[nodiscard]] bool in_cooldown() const;

private:
    core::Engine& engine_;
    core::Duration cooldown_period_;
    core::TimePoint cooldown_until_{core::Duration{0.0}};
};

// Power-aware DVFS policy: frequency scales with active utilization
// Formula: f = f_min + (f_max - f_min) * U_active
class PowerAwareDvfsPolicy : public DvfsPolicy {
public:
    explicit PowerAwareDvfsPolicy(core::Engine& engine,
                                   core::Duration cooldown = core::Duration{0.0});

    void on_utilization_changed(EdfScheduler& scheduler, core::ClockDomain& domain) override;
    void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) override;
    void on_processor_active(EdfScheduler& scheduler, core::Processor& proc) override;
    [[nodiscard]] core::Duration cooldown_period() const override { return cooldown_; }

private:
    [[nodiscard]] core::Frequency compute_target_frequency(
        const core::ClockDomain& domain, double active_util) const;
    void apply_frequency_change(core::ClockDomain& domain, core::Frequency target);
    [[nodiscard]] CooldownTimer& get_cooldown_timer(core::ClockDomain& domain);

    core::Engine& engine_;
    core::Duration cooldown_;
    std::unordered_map<std::size_t, CooldownTimer> domain_cooldowns_;
};

} // namespace schedsim::algo
