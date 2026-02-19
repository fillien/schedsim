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

/// @brief Groups processors that share a common frequency setting.
/// @ingroup core_hardware
///
/// A ClockDomain models a voltage/frequency island: all processors within it
/// operate at the same frequency. DVFS operations (set_frequency()) affect
/// every processor in the domain simultaneously.
///
/// Optionally, a ClockDomain can be configured with discrete Operating
/// Performance Points (OPPs) via set_frequency_modes(), and with a polynomial
/// power model via set_power_coefficients() for energy tracking.
///
/// ClockDomain is non-copyable but movable (Decision 61).
///
/// @see Processor, PowerDomain, EnergyTracker
class ClockDomain {
public:
    /// @brief Construct a ClockDomain with its frequency bounds.
    /// @param id                Unique identifier within the platform.
    /// @param freq_min          Minimum allowed frequency for this domain.
    /// @param freq_max          Maximum allowed frequency for this domain.
    ///                          The domain is initialised to this frequency.
    /// @param transition_delay  Duration of an asynchronous DVFS transition.
    ///                          A zero duration means frequency changes are
    ///                          applied instantaneously.
    ClockDomain(std::size_t id, Frequency freq_min, Frequency freq_max,
                Duration transition_delay = duration_from_seconds(0.0));

    /// @brief Return the unique identifier of this clock domain.
    /// @return Clock domain ID.
    [[nodiscard]] std::size_t id() const noexcept { return id_; }

    /// @brief Return the current operating frequency.
    /// @return The active frequency of all processors in this domain.
    [[nodiscard]] Frequency frequency() const noexcept { return current_freq_; }

    /// @brief Return the minimum allowed frequency.
    /// @return Lower bound of the frequency range.
    [[nodiscard]] Frequency freq_min() const noexcept { return freq_min_; }

    /// @brief Return the maximum allowed frequency.
    /// @return Upper bound of the frequency range.
    [[nodiscard]] Frequency freq_max() const noexcept { return freq_max_; }

    /// @brief Return the DVFS transition delay.
    /// @return Duration of an asynchronous frequency change. Zero means
    ///         transitions are instantaneous.
    [[nodiscard]] Duration transition_delay() const noexcept { return transition_delay_; }

    /// @brief Check whether DVFS is permanently disabled for this domain.
    /// @return True if lock_frequency() has been called.
    /// @see lock_frequency
    [[nodiscard]] bool is_locked() const noexcept { return locked_; }

    /// @brief Check whether an asynchronous DVFS transition is in progress.
    /// @return True if a frequency change has been initiated but not yet completed.
    [[nodiscard]] bool is_transitioning() const noexcept { return transitioning_; }

    /// @brief Return the processors belonging to this clock domain.
    /// @return A read-only span of processor pointers.
    /// @see Processor
    [[nodiscard]] std::span<Processor* const> processors() const noexcept;

    /// @brief Set the operating frequency of this clock domain.
    ///
    /// If transition_delay is zero the change takes effect immediately and all
    /// processors are notified synchronously. Otherwise, an asynchronous DVFS
    /// transition is started: each processor enters the Changing state for the
    /// duration of the transition.
    ///
    /// @param freq  The target frequency.
    /// @throws OutOfRangeError   if @p freq is outside [freq_min, freq_max].
    /// @throws InvalidStateError if the domain is locked or already
    ///         transitioning.
    /// @see lock_frequency, is_transitioning
    void set_frequency(Frequency freq);

    /// @brief Permanently disable DVFS for this domain.
    ///
    /// After this call, set_frequency() will throw InvalidStateError.
    /// @see is_locked
    void lock_frequency() noexcept { locked_ = true; }

    /// @brief Define the set of discrete Operating Performance Points (OPPs).
    ///
    /// When OPPs are set, set_frequency() only accepts values that exactly
    /// match one of the modes, and ceil_to_mode() can be used to round up.
    ///
    /// @param modes  A list of valid frequencies, sorted ascending.
    /// @see frequency_modes, has_frequency_modes, ceil_to_mode
    void set_frequency_modes(std::vector<Frequency> modes);

    /// @brief Return the discrete OPP frequency list.
    /// @return A span of the configured frequency modes, or an empty span if
    ///         continuous DVFS is in use.
    /// @see set_frequency_modes
    [[nodiscard]] std::span<const Frequency> frequency_modes() const noexcept;

    /// @brief Check whether discrete OPPs have been configured.
    /// @return True if set_frequency_modes() has been called with a non-empty
    ///         list.
    /// @see set_frequency_modes
    [[nodiscard]] bool has_frequency_modes() const noexcept;

    /// @brief Round a frequency up to the nearest configured OPP.
    ///
    /// Returns the smallest frequency mode that is greater than or equal to
    /// @p freq. If @p freq exceeds all modes, returns the highest mode.
    ///
    /// @param freq  The frequency to round up.
    /// @return The ceiling OPP frequency.
    /// @see set_frequency_modes
    [[nodiscard]] Frequency ceil_to_mode(Frequency freq) const;

    /// @brief Set the efficient frequency threshold for DPM decisions.
    ///
    /// Below this frequency it may be more energy-efficient to use C-states
    /// (race-to-idle) rather than running at a very low frequency.
    ///
    /// @param freq  The efficient frequency threshold.
    /// @see freq_eff
    void set_freq_eff(Frequency freq) noexcept { freq_eff_ = freq; }

    /// @brief Return the efficient frequency threshold.
    /// @return The frequency below which DPM sleep may be preferable to DVFS.
    /// @see set_freq_eff
    [[nodiscard]] Frequency freq_eff() const noexcept { return freq_eff_; }

    /// @brief Set the polynomial power-model coefficients.
    ///
    /// The power model is: `P(f) = a0 + a1*f + a2*f^2 + a3*f^3`, where
    /// power is in milliwatts and frequency is in GHz.
    ///
    /// @param coeffs  Array of four coefficients {a0, a1, a2, a3}.
    /// @see power_at_frequency, EnergyTracker
    void set_power_coefficients(std::array<double, 4> coeffs) noexcept;

    /// @brief Evaluate the power model at a given frequency.
    /// @param freq  The frequency at which to compute power.
    /// @return Estimated power consumption (mW) at the specified frequency.
    /// @see set_power_coefficients
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

    // Discrete frequency modes (sorted ascending, empty = continuous)
    std::vector<Frequency> frequency_modes_;
    Frequency freq_eff_{0.0};
};

} // namespace schedsim::core
