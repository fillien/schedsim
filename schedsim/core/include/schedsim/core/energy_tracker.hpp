#pragma once

#include <schedsim/core/processor.hpp>
#include <schedsim/core/types.hpp>

#include <cstddef>
#include <vector>

namespace schedsim::core {

class ClockDomain;
class Platform;

// EnergyTracker monitors power consumption and accumulates energy over time.
// It tracks state changes of processors, clock domains, and power domains
// to compute power draw based on the current configuration.
class EnergyTracker {
public:
    explicit EnergyTracker(Platform& platform, TimePoint start_time);

    // Update hooks - called by processors and clock domains on state changes
    void on_processor_state_change(Processor& proc, ProcessorState old_state,
                                   ProcessorState new_state, TimePoint now);
    void on_frequency_change(ClockDomain& cd, Frequency old_freq,
                             Frequency new_freq, TimePoint now);
    void on_cstate_change(Processor& proc, int old_level, int new_level,
                          TimePoint now);

    // Bring all accumulators up to the current time
    void update_to_time(TimePoint now);

    // Energy queries
    [[nodiscard]] Energy processor_energy(std::size_t proc_id) const;
    [[nodiscard]] Energy clock_domain_energy(std::size_t cd_id) const;
    [[nodiscard]] Energy power_domain_energy(std::size_t pd_id) const;
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
