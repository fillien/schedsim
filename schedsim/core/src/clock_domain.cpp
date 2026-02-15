#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/event.hpp>
#include <schedsim/core/processor.hpp>

#include <algorithm>
#include <cmath>

namespace schedsim::core {

ClockDomain::ClockDomain(std::size_t id, Frequency freq_min, Frequency freq_max,
                         Duration transition_delay)
    : id_(id)
    , current_freq_(freq_max)  // Start at max frequency
    , freq_min_(freq_min)
    , freq_max_(freq_max)
    , transition_delay_(transition_delay) {}

std::span<Processor* const> ClockDomain::processors() const noexcept {
    return processors_;
}

void ClockDomain::set_frequency(Frequency freq) {
    if (locked_) {
        throw InvalidStateError("Cannot change frequency on locked clock domain");
    }
    if (transitioning_) {
        throw InvalidStateError("Cannot change frequency during DVFS transition");
    }
    if (freq.mhz < freq_min_.mhz || freq.mhz > freq_max_.mhz) {
        throw OutOfRangeError("Frequency out of range");
    }

    // If no delay or no engine, apply immediately
    if (transition_delay_.count() <= 0.0 || !engine_) {
        Frequency old_freq = current_freq_;
        current_freq_ = freq;
        // Notify energy tracker of immediate frequency change
        if (engine_) {
            engine_->notify_frequency_change(*this, old_freq, freq);
        }
        return;
    }

    // Begin async DVFS transition
    transitioning_ = true;
    pending_freq_ = freq;

    // Notify all processors that DVFS is starting
    for (Processor* proc : processors_) {
        proc->begin_dvfs();
    }

    // Schedule DVFS completion timer
    TimePoint complete_time = engine_->time() + transition_delay_;
    dvfs_timer_ = engine_->add_timer(
        complete_time,
        EventPriority::PROCESSOR_AVAILABLE,
        [this]() { on_dvfs_complete(); }
    );
}

void ClockDomain::set_power_coefficients(std::array<double, 4> coeffs) noexcept {
    power_coefficients_ = coeffs;
}

Power ClockDomain::power_at_frequency(Frequency freq) const noexcept {
    // Convert frequency to GHz for polynomial evaluation
    double f_ghz = freq.mhz / 1000.0;

    // P(f) = a0 + a1*f + a2*f^2 + a3*f^3
    double power = power_coefficients_[0]
                 + power_coefficients_[1] * f_ghz
                 + power_coefficients_[2] * f_ghz * f_ghz
                 + power_coefficients_[3] * f_ghz * f_ghz * f_ghz;

    return Power{power};
}

void ClockDomain::on_dvfs_complete() {
    dvfs_timer_.clear();

    if (!transitioning_) {
        return;
    }

    Frequency old_freq = current_freq_;

    // Apply the new frequency
    current_freq_ = pending_freq_;
    transitioning_ = false;

    // Notify energy tracker of frequency change
    if (engine_) {
        engine_->notify_frequency_change(*this, old_freq, pending_freq_);
    }

    // Notify all processors that DVFS is complete
    for (Processor* proc : processors_) {
        proc->end_dvfs();
    }
}

void ClockDomain::set_frequency_modes(std::vector<Frequency> modes) {
    // Sort ascending by MHz
    std::sort(modes.begin(), modes.end(),
              [](const Frequency& a, const Frequency& b) { return a.mhz < b.mhz; });
    // Deduplicate
    modes.erase(std::unique(modes.begin(), modes.end()), modes.end());
    frequency_modes_ = std::move(modes);
    // Update min/max from sorted modes
    if (!frequency_modes_.empty()) {
        freq_min_ = frequency_modes_.front();
        freq_max_ = frequency_modes_.back();
    }
}

std::span<const Frequency> ClockDomain::frequency_modes() const noexcept {
    return frequency_modes_;
}

bool ClockDomain::has_frequency_modes() const noexcept {
    return !frequency_modes_.empty();
}

Frequency ClockDomain::ceil_to_mode(Frequency freq) const {
    if (frequency_modes_.empty()) {
        // Continuous mode: clamp to [freq_min, freq_max]
        return Frequency{std::clamp(freq.mhz, freq_min_.mhz, freq_max_.mhz)};
    }
    // Find smallest mode >= freq (lower_bound on ascending modes)
    auto it = std::lower_bound(
        frequency_modes_.begin(), frequency_modes_.end(), freq,
        [](const Frequency& mode, const Frequency& target) { return mode.mhz < target.mhz; });
    if (it != frequency_modes_.end()) {
        return *it;
    }
    // All modes are below freq — return max
    return frequency_modes_.back();
}

void ClockDomain::add_processor(Processor* proc) {
    processors_.push_back(proc);
}

} // namespace schedsim::core
