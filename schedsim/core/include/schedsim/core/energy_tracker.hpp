#pragma once

#include <schedsim/core/processor.hpp>
#include <schedsim/core/types.hpp>

#include <cstddef>
#include <vector>

namespace schedsim::core {

class ClockDomain;
class Platform;

/// @brief Monitors power consumption and accumulates energy over time.
/// @ingroup core_hardware
///
/// EnergyTracker listens for state-change notifications from Processor and
/// ClockDomain objects and uses the configured power models to integrate
/// instantaneous power into cumulative energy. It maintains per-processor
/// accumulators that can be queried individually or aggregated by clock
/// domain, power domain, or across the entire platform.
///
/// Typical usage:
/// 1. Construct with a Platform reference and the simulation start time.
/// 2. Wire up the `on_*` hooks so that Processor and ClockDomain call them
///    on every state change.
/// 3. At the end of the simulation (or at any point), call update_to_time()
///    and then read energy via the query methods.
///
/// @see ClockDomain::set_power_coefficients, PowerDomain::cstate_power,
///      Processor, ProcessorState
class EnergyTracker {
public:
    /// @brief Construct an EnergyTracker bound to a platform.
    /// @param platform    The Platform whose processors and domains are tracked.
    /// @param start_time  The simulation time at which tracking begins. Energy
    ///                    is accumulated from this point forward.
    explicit EnergyTracker(Platform& platform, TimePoint start_time);

    /// @brief Notify the tracker that a processor has changed execution state.
    ///
    /// The tracker closes the previous power interval for the processor and
    /// opens a new one based on the new state.
    ///
    /// @param proc       The processor whose state changed.
    /// @param old_state  The state the processor was in before the change.
    /// @param new_state  The state the processor is now in.
    /// @param now        The simulation time at which the change occurred.
    /// @see ProcessorState
    void on_processor_state_change(Processor& proc, ProcessorState old_state,
                                   ProcessorState new_state, TimePoint now);

    /// @brief Notify the tracker that a clock domain has changed frequency.
    ///
    /// The tracker closes the previous power interval for every processor in
    /// the clock domain and opens new intervals at the new frequency.
    ///
    /// @param cd        The clock domain whose frequency changed.
    /// @param old_freq  The frequency before the change.
    /// @param new_freq  The frequency after the change.
    /// @param now       The simulation time at which the change occurred.
    /// @see ClockDomain::set_frequency
    void on_frequency_change(ClockDomain& cd, Frequency old_freq,
                             Frequency new_freq, TimePoint now);

    /// @brief Notify the tracker that a processor's C-state level has changed.
    ///
    /// The tracker adjusts the power model for the processor based on the new
    /// C-state's power consumption.
    ///
    /// @param proc       The processor whose C-state changed.
    /// @param old_level  The C-state level before the change (0 = C0/active).
    /// @param new_level  The C-state level after the change.
    /// @param now        The simulation time at which the change occurred.
    /// @see Processor::request_cstate, PowerDomain
    void on_cstate_change(Processor& proc, int old_level, int new_level,
                          TimePoint now);

    /// @brief Flush all accumulators up to the specified simulation time.
    ///
    /// Computes the energy consumed between each processor's last-update time
    /// and @p now, then advances all last-update timestamps. Call this before
    /// reading energy values to ensure they reflect the latest state.
    ///
    /// @param now  The simulation time to advance to.
    void update_to_time(TimePoint now);

    /// @brief Return the accumulated energy for a single processor.
    /// @param proc_id  The processor's unique identifier.
    /// @return Accumulated energy (mJ) from start_time to the last update.
    /// @see update_to_time
    [[nodiscard]] Energy processor_energy(std::size_t proc_id) const;

    /// @brief Return the accumulated energy for all processors in a clock domain.
    /// @param cd_id  The clock domain's unique identifier.
    /// @return Sum of accumulated energy (mJ) across all processors in the domain.
    /// @see ClockDomain
    [[nodiscard]] Energy clock_domain_energy(std::size_t cd_id) const;

    /// @brief Return the accumulated energy for all processors in a power domain.
    /// @param pd_id  The power domain's unique identifier.
    /// @return Sum of accumulated energy (mJ) across all processors in the domain.
    /// @see PowerDomain
    [[nodiscard]] Energy power_domain_energy(std::size_t pd_id) const;

    /// @brief Return the total accumulated energy across the entire platform.
    /// @return Sum of accumulated energy (mJ) for every processor.
    [[nodiscard]] Energy total_energy() const;

private:
    struct ProcessorEnergyState {
        Energy accumulated{0.0};
        TimePoint last_update;
        ProcessorState last_state{ProcessorState::Idle};
        int last_cstate_level{0};
    };

    [[nodiscard]] Power compute_processor_power(const Processor& proc,
                                                ProcessorState state,
                                                int cstate_level) const;

    Platform& platform_;
    std::vector<ProcessorEnergyState> processor_states_;
};

} // namespace schedsim::core
