#pragma once

#include <schedsim/core/timer.hpp>
#include <schedsim/core/types.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace schedsim::core {

class Engine;
class Processor;

// ClockDomain groups processors that share a frequency setting.
// DVFS operations affect all processors in a clock domain.
class ClockDomain {
public:
    ClockDomain(std::size_t id, Frequency freq_min, Frequency freq_max,
                Duration transition_delay = Duration{0.0});

    [[nodiscard]] std::size_t id() const noexcept { return id_; }
    [[nodiscard]] Frequency frequency() const noexcept { return current_freq_; }
    [[nodiscard]] Frequency freq_min() const noexcept { return freq_min_; }
    [[nodiscard]] Frequency freq_max() const noexcept { return freq_max_; }
    [[nodiscard]] Duration transition_delay() const noexcept { return transition_delay_; }
    [[nodiscard]] bool is_locked() const noexcept { return locked_; }
    [[nodiscard]] bool is_transitioning() const noexcept { return transitioning_; }
    [[nodiscard]] std::span<Processor* const> processors() const noexcept;

    // Set current frequency. Throws OutOfRangeError if out of range.
    // Throws InvalidStateError if locked or transitioning.
    // If transition_delay > 0, begins async DVFS transition.
    void set_frequency(Frequency freq);

    // Permanently disable DVFS for this domain
    void lock_frequency() noexcept { locked_ = true; }

    // Power model for energy tracking: P(f) = a0 + a1*f + a2*f^2 + a3*f^3
    // Coefficients: {a0, a1, a2, a3}, power in mW, frequency in GHz
    void set_power_coefficients(std::array<double, 4> coeffs) noexcept;
    [[nodiscard]] Power power_at_frequency(Frequency freq) const noexcept;

    // Non-copyable, movable (Decision 61)
    ClockDomain(const ClockDomain&) = delete;
    ClockDomain& operator=(const ClockDomain&) = delete;
    ClockDomain(ClockDomain&&) = default;
    ClockDomain& operator=(ClockDomain&&) = default;

private:
    friend class Platform;

    void add_processor(Processor* proc);
    void set_engine(Engine* engine) noexcept { engine_ = engine; }
    void on_dvfs_complete();

    std::size_t id_;
    Frequency current_freq_;
    Frequency freq_min_;
    Frequency freq_max_;
    Duration transition_delay_;
    bool locked_{false};
    bool transitioning_{false};
    Frequency pending_freq_{0.0};
    TimerId dvfs_timer_;
    std::array<double, 4> power_coefficients_{0.0, 0.0, 0.0, 0.0};
    std::vector<Processor*> processors_;
    Engine* engine_{nullptr};
};

} // namespace schedsim::core
