#pragma once

#include <schedsim/core/types.hpp>

#include <unordered_set>

namespace schedsim::core {
class Processor;
} // namespace schedsim::core

namespace schedsim::algo {

class EdfScheduler;

// Interface for DPM (Dynamic Power Management) policies
// DPM policies manage processor sleep states (C-states) to reduce power consumption
class DpmPolicy {
public:
    virtual ~DpmPolicy() = default;

    // Called when a processor becomes idle
    // Policy should decide whether to put the processor to sleep
    virtual void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) = 0;

    // Called when a processor is needed (jobs are waiting to be scheduled)
    // Policy should wake up sleeping processors if needed
    virtual void on_processor_needed(EdfScheduler& scheduler) = 0;
};

// Basic DPM policy: puts idle processors to sleep immediately
// Wakes processors when jobs arrive
class BasicDpmPolicy : public DpmPolicy {
public:
    // target_cstate: the C-state level to enter when idle (1 = C1, 2 = C2, etc.)
    // idle_threshold: minimum idle time before entering sleep (0 = immediate)
    explicit BasicDpmPolicy(int target_cstate = 1,
                            core::Duration idle_threshold = core::duration_from_seconds(0.0));

    void on_processor_idle(EdfScheduler& scheduler, core::Processor& proc) override;
    void on_processor_needed(EdfScheduler& scheduler) override;

    // Accessors
    [[nodiscard]] int target_cstate() const noexcept { return target_cstate_; }
    [[nodiscard]] core::Duration idle_threshold() const noexcept { return idle_threshold_; }
    [[nodiscard]] std::size_t sleeping_processor_count() const noexcept {
        return sleeping_processors_.size();
    }

private:
    int target_cstate_;
    core::Duration idle_threshold_;
    std::unordered_set<core::Processor*> sleeping_processors_;
};

} // namespace schedsim::algo
