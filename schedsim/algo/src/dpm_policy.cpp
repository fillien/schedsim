#include <schedsim/algo/dpm_policy.hpp>

#include <schedsim/algo/edf_scheduler.hpp>

#include <schedsim/core/processor.hpp>

namespace schedsim::algo {

BasicDpmPolicy::BasicDpmPolicy(int target_cstate, core::Duration idle_threshold)
    : target_cstate_(target_cstate)
    , idle_threshold_(idle_threshold) {}

void BasicDpmPolicy::on_processor_idle(EdfScheduler& /*scheduler*/, core::Processor& proc) {
    // Check if processor is already sleeping
    if (proc.state() == core::ProcessorState::Sleep) {
        return;
    }

    // Check if processor is idle
    if (proc.state() != core::ProcessorState::Idle) {
        return;
    }

    // For simplicity, we ignore idle_threshold and enter sleep immediately
    // A more sophisticated implementation could schedule a timer
    if (idle_threshold_.count() == 0.0) {
        proc.request_cstate(target_cstate_);
        sleeping_processors_.insert(&proc);
    }
    // TODO: If idle_threshold > 0, schedule a timer and check again
}

void BasicDpmPolicy::on_processor_needed(EdfScheduler& /*scheduler*/) {
    // Wake up one sleeping processor if there are any
    // The processor will be woken by assigning a job to it,
    // which triggers the wake-up sequence in Library 1

    // Find a sleeping processor to wake
    for (auto* proc : sleeping_processors_) {
        // Check if still sleeping (might have been woken by other means)
        if (proc->state() == core::ProcessorState::Sleep) {
            // The processor will be woken when assign() is called on it
            // Just track that we know about it
            break;
        }
    }

    // Clean up processors that are no longer sleeping
    for (auto it = sleeping_processors_.begin(); it != sleeping_processors_.end();) {
        if ((*it)->state() != core::ProcessorState::Sleep) {
            it = sleeping_processors_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace schedsim::algo
