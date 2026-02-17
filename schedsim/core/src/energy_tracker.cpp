#include <schedsim/core/energy_tracker.hpp>
#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor.hpp>

namespace schedsim::core {

EnergyTracker::EnergyTracker(Platform& platform, TimePoint start_time)
    : platform_(platform)
    , processor_states_(platform.processor_count()) {
    // Initialize all processor states
    for (std::size_t i = 0; i < platform.processor_count(); ++i) {
        processor_states_[i].last_update = start_time;
        processor_states_[i].last_state = platform.processor(i).state();
        processor_states_[i].last_cstate_level = platform.processor(i).current_cstate_level();
    }
}

void EnergyTracker::on_processor_state_change(Processor& proc, ProcessorState old_state,
                                              ProcessorState new_state, TimePoint now) {
    if (proc.id() >= processor_states_.size()) {
        return;
    }

    auto& ps = processor_states_[proc.id()];

    // Accumulate energy for time spent in old state
    Duration elapsed = now - ps.last_update;
    if (elapsed > Duration::zero()) {
        Power power = compute_processor_power(proc, old_state, ps.last_cstate_level);
        // Energy (mJ) = Power (mW) * Time (s)
        ps.accumulated.mj += power.mw * duration_to_seconds(elapsed);
    }

    // Update state
    ps.last_update = now;
    ps.last_state = new_state;
}

void EnergyTracker::on_frequency_change(ClockDomain& cd, Frequency old_freq,
                                        Frequency new_freq, TimePoint now) {
    (void)new_freq;

    // Update energy for all processors in this clock domain
    for (Processor* proc : cd.processors()) {
        if (proc->id() >= processor_states_.size()) {
            continue;
        }

        auto& ps = processor_states_[proc->id()];

        // Accumulate energy at old frequency
        Duration elapsed = now - ps.last_update;
        if (elapsed > Duration::zero()) {
            // Use old_freq for power calculation since current_freq has already changed
            Power power;
            if (ps.last_state == ProcessorState::Sleep) {
                power = proc->power_domain().cstate_power(ps.last_cstate_level);
            } else {
                power = cd.power_at_frequency(old_freq);
            }
            ps.accumulated.mj += power.mw * duration_to_seconds(elapsed);
        }

        ps.last_update = now;
    }
}

void EnergyTracker::on_cstate_change(Processor& proc, int old_level, int new_level,
                                     TimePoint now) {
    if (proc.id() >= processor_states_.size()) {
        return;
    }

    auto& ps = processor_states_[proc.id()];

    // Accumulate energy at old C-state level
    Duration elapsed = now - ps.last_update;
    if (elapsed > Duration::zero()) {
        Power power = compute_processor_power(proc, ps.last_state, old_level);
        ps.accumulated.mj += power.mw * duration_to_seconds(elapsed);
    }

    // Update state
    ps.last_update = now;
    ps.last_cstate_level = new_level;
}

void EnergyTracker::update_to_time(TimePoint now) {
    for (std::size_t i = 0; i < platform_.processor_count(); ++i) {
        auto& ps = processor_states_[i];
        const Processor& proc = platform_.processor(i);

        Duration elapsed = now - ps.last_update;
        if (elapsed > Duration::zero()) {
            // Use the processor's current state for power computation
            Power power = compute_processor_power(proc, proc.state(), proc.current_cstate_level());
            ps.accumulated.mj += power.mw * duration_to_seconds(elapsed);
            ps.last_update = now;
        }
    }
}

Energy EnergyTracker::processor_energy(std::size_t proc_id) const {
    if (proc_id >= processor_states_.size()) {
        return Energy{0.0};
    }
    return processor_states_[proc_id].accumulated;
}

Energy EnergyTracker::clock_domain_energy(std::size_t cd_id) const {
    if (cd_id >= platform_.clock_domain_count()) {
        return Energy{0.0};
    }

    Energy total{0.0};
    const ClockDomain& cd = platform_.clock_domain(cd_id);
    for (const Processor* proc : cd.processors()) {
        if (proc->id() < processor_states_.size()) {
            total.mj += processor_states_[proc->id()].accumulated.mj;
        }
    }
    return total;
}

Energy EnergyTracker::power_domain_energy(std::size_t pd_id) const {
    if (pd_id >= platform_.power_domain_count()) {
        return Energy{0.0};
    }

    Energy total{0.0};
    const PowerDomain& pd = platform_.power_domain(pd_id);
    for (const Processor* proc : pd.processors()) {
        if (proc->id() < processor_states_.size()) {
            total.mj += processor_states_[proc->id()].accumulated.mj;
        }
    }
    return total;
}

Energy EnergyTracker::total_energy() const {
    Energy total{0.0};
    for (const auto& ps : processor_states_) {
        total.mj += ps.accumulated.mj;
    }
    return total;
}

Power EnergyTracker::compute_processor_power(const Processor& proc,
                                             ProcessorState state,
                                             int cstate_level) const {
    // Sleep states use C-state power
    if (state == ProcessorState::Sleep) {
        return proc.power_domain().cstate_power(cstate_level);
    }

    // All active states (Idle, Running, ContextSwitching, Changing) use the
    // same frequency-based power: P(f) = a0 + a1*f + a2*f^2 + a3*f^3.
    // This is why only Sleep<->Active transitions need to notify the
    // EnergyTracker — transitions between active states don't change power.
    const ClockDomain& cd = proc.clock_domain();
    return cd.power_at_frequency(cd.frequency());
}

} // namespace schedsim::core
