#include <schedsim/core/processor.hpp>
#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor_type.hpp>
#include <schedsim/core/task.hpp>

#include <utility>

namespace schedsim::core {

Processor::Processor(std::size_t id, ProcessorType& type,
                     ClockDomain& clock_domain, PowerDomain& power_domain)
    : id_(id)
    , type_(&type)
    , clock_domain_(&clock_domain)
    , power_domain_(&power_domain) {}

Task* Processor::current_task() const noexcept {
    return current_job_ ? &current_job_->task() : nullptr;
}

double Processor::speed(double reference_performance) const noexcept {
    // Normalized speed: (freq/freq_max) * (perf/perf_ref)
    double freq_ratio = clock_domain_->frequency().mhz / clock_domain_->freq_max().mhz;
    double perf_ratio = type_->performance() / reference_performance;
    return freq_ratio * perf_ratio;
}

void Processor::assign(Job& job) {
    // Allow assign from Idle or Sleep states
    if (state_ != ProcessorState::Idle && state_ != ProcessorState::Sleep) {
        throw InvalidStateError("Cannot assign job: processor not idle or sleeping");
    }

    // If sleeping, start wake-up sequence
    if (state_ == ProcessorState::Sleep) {
        begin_wake_up(job);
        return;
    }

    // From Idle: check if context switch is enabled and has non-zero delay
    if (engine_ && engine_->context_switch_enabled()) {
        Duration cs_delay = type_->context_switch_delay();
        if (cs_delay > Duration::zero()) {
            begin_context_switch(job);
            return;
        }
    }

    // Direct transition to Running (CS disabled or zero delay)
    current_job_ = &job;
    state_ = ProcessorState::Running;

    if (engine_) {
        last_update_time_ = engine_->time();
        schedule_completion();

        // Schedule deadline timer (at deadline time or current time if passed)
        // Using std::max ensures deadline misses are detected even for past deadlines
        core::TimePoint deadline_time = std::max(job.absolute_deadline(), engine_->time());
        deadline_timer_ = engine_->add_timer(
            deadline_time,
            EventPriority::DEADLINE_MISS,
            [this]() { on_deadline_timer(); }
        );
    }
}

void Processor::clear() {
    if (state_ == ProcessorState::Idle) {
        throw InvalidStateError("Cannot clear: processor already idle");
    }

    // Handle clear() during context switching
    if (state_ == ProcessorState::ContextSwitching) {
        if (engine_) {
            engine_->cancel_timer(transition_timer_);
            engine_->cancel_timer(deadline_timer_);
        }
        pending_job_ = nullptr;
        state_ = ProcessorState::Idle;
        return;
    }

    // Handle clear() during DVFS transition
    if (state_ == ProcessorState::Changing) {
        // Mark for deferred handling - DVFS will complete, then return to Idle
        pending_clear_ = true;
        return;
    }

    // Update consumed work before clearing (only when Running)
    if (state_ == ProcessorState::Running && current_job_) {
        update_consumed_work();
    }

    // Cancel pending timers
    cancel_completion();
    if (engine_) {
        engine_->cancel_timer(deadline_timer_);
    }

    current_job_ = nullptr;
    state_ = ProcessorState::Idle;
}

void Processor::request_cstate(int level) {
    if (state_ == ProcessorState::Running ||
        state_ == ProcessorState::Changing ||
        state_ == ProcessorState::ContextSwitching) {
        throw InvalidStateError("Cannot enter C-state while running, changing, or context switching");
    }

    ProcessorState old_state = state_;
    current_cstate_level_ = level;
    state_ = ProcessorState::Sleep;

    // Only Sleep<->Active transitions notify the EnergyTracker because
    // compute_processor_power() treats all active states (Idle, Running,
    // ContextSwitching, Changing) identically — they all use frequency-based
    // power P(f). Only Sleep uses distinct C-state power.
    if (engine_) {
        engine_->notify_processor_state_change(*this, old_state, ProcessorState::Sleep);
    }
}

void Processor::set_job_completion_handler(JobCompletionHandler handler) {
    on_job_completion_ = std::move(handler);
}

void Processor::set_deadline_miss_handler(DeadlineMissHandler handler) {
    on_deadline_miss_ = std::move(handler);
}

void Processor::set_processor_available_handler(ProcessorAvailableHandler handler) {
    on_processor_available_ = std::move(handler);
}

void Processor::on_completion_timer() {
    completion_timer_.clear();

    if (!current_job_ || state_ != ProcessorState::Running) {
        return;
    }

    // Update consumed work - job should be complete
    update_consumed_work();

    Job* completed_job = current_job_;

    // Cancel deadline timer
    if (engine_) {
        engine_->cancel_timer(deadline_timer_);
    }

    current_job_ = nullptr;
    state_ = ProcessorState::Idle;

    // Notify handler
    if (on_job_completion_ && completed_job) {
        on_job_completion_(*this, *completed_job);
    }
}

void Processor::on_deadline_timer() {
    deadline_timer_.clear();

    if (!current_job_) {
        return;
    }

    // Update consumed work
    if (state_ == ProcessorState::Running) {
        update_consumed_work();
    }

    // Notify handler
    if (on_deadline_miss_ && current_job_) {
        on_deadline_miss_(*this, *current_job_);
    }
}

void Processor::schedule_completion() {
    if (!engine_ || !current_job_) {
        return;
    }

    // Calculate time to completion based on remaining work and speed
    Duration remaining = current_job_->remaining_work();

    if (remaining <= Duration::zero()) {
        // Job already complete (floating-point rounding from DVFS) — fire immediately
        on_completion_timer();
        return;
    }

    double spd = speed(reference_performance_);

    if (spd <= 0.0) {
        return;  // Can't complete if speed is zero
    }

    // Wall time = reference_work / speed
    Duration wall_time = divide_duration(remaining, spd);
    TimePoint completion_time = engine_->time() + wall_time;

    completion_timer_ = engine_->add_timer(
        completion_time,
        EventPriority::JOB_COMPLETION,
        [this]() { on_completion_timer(); }
    );
}

void Processor::cancel_completion() {
    if (engine_) {
        engine_->cancel_timer(completion_timer_);
    }
}

void Processor::update_consumed_work() {
    if (!engine_ || !current_job_ || state_ != ProcessorState::Running) {
        return;
    }

    TimePoint now = engine_->time();
    Duration elapsed = now - last_update_time_;

    // Work consumed = elapsed_wall_time * speed (in reference units)
    double spd = speed(reference_performance_);
    Duration work_done = scale_duration(elapsed, spd);

    current_job_->consume_work(work_done);
    last_update_time_ = now;
}

void Processor::reschedule_completion() {
    cancel_completion();
    if (engine_ && current_job_ && state_ == ProcessorState::Running) {
        last_update_time_ = engine_->time();
        schedule_completion();
    }
}

void Processor::notify_immediate_freq_change() {
    // Lightweight notification for zero-delay frequency changes.
    // Just updates consumed work and reschedules completion without
    // changing state or firing ProcessorAvailable ISR.
    if (state_ == ProcessorState::Running && current_job_) {
        update_consumed_work();
        cancel_completion();
        schedule_completion();
    }
}

void Processor::begin_context_switch(Job& job) {
    pending_job_ = &job;
    state_ = ProcessorState::ContextSwitching;

    if (engine_) {
        // Cancel any existing deadline timer from wake-up phase to prevent duplicates
        engine_->cancel_timer(deadline_timer_);

        Duration cs_delay = type_->context_switch_delay();
        TimePoint complete_time = engine_->time() + cs_delay;

        // Only schedule context switch completion - NOT deadline timer
        // Deadline timer is scheduled in on_context_switch_complete() after
        // current_job_ is set, so on_deadline_timer() can properly detect misses
        transition_timer_ = engine_->add_timer(
            complete_time,
            EventPriority::PROCESSOR_AVAILABLE,
            [this]() { on_context_switch_complete(); }
        );
    }
}

void Processor::on_context_switch_complete() {
    transition_timer_.clear();

    if (state_ != ProcessorState::ContextSwitching || !pending_job_) {
        return;
    }

    // Transition to Running
    current_job_ = pending_job_;
    pending_job_ = nullptr;
    state_ = ProcessorState::Running;

    if (engine_) {
        last_update_time_ = engine_->time();
        schedule_completion();

        // Schedule deadline timer HERE, after current_job_ is set
        // This ensures on_deadline_timer() can properly detect the miss
        TimePoint deadline_time = std::max(current_job_->absolute_deadline(), engine_->time());
        deadline_timer_ = engine_->add_timer(
            deadline_time,
            EventPriority::DEADLINE_MISS,
            [this]() { on_deadline_timer(); }
        );
    }

    // Fire ProcessorAvailable ISR
    if (on_processor_available_) {
        on_processor_available_(*this);
    }
}

void Processor::begin_wake_up(Job& job) {
    pending_job_ = &job;

    if (!engine_) {
        return;
    }

    // Get wake-up latency based on current C-state level
    Duration wake_latency = power_domain_->wake_latency(current_cstate_level_);

    if (wake_latency <= Duration::zero()) {
        // No wake-up latency - immediate transition
        current_cstate_level_ = 0;
        state_ = ProcessorState::Idle;

        if (engine_) {
            engine_->notify_processor_state_change(*this, ProcessorState::Sleep, ProcessorState::Idle);
        }

        // Now continue with normal assign flow (may trigger context switch)
        Job* saved_pending = pending_job_;
        pending_job_ = nullptr;
        assign(*saved_pending);
        return;
    }

    // Schedule deadline timer (at deadline time or current time if passed)
    // Using std::max ensures deadline misses are detected even for past deadlines
    core::TimePoint deadline_time = std::max(job.absolute_deadline(), engine_->time());
    deadline_timer_ = engine_->add_timer(
        deadline_time,
        EventPriority::DEADLINE_MISS,
        [this]() { on_deadline_timer(); }
    );

    // Schedule wake-up completion timer
    TimePoint wake_complete_time = engine_->time() + wake_latency;
    transition_timer_ = engine_->add_timer(
        wake_complete_time,
        EventPriority::PROCESSOR_AVAILABLE,
        [this]() { on_wake_up_complete(); }
    );
}

void Processor::on_wake_up_complete() {
    transition_timer_.clear();

    if (state_ != ProcessorState::Sleep || !pending_job_) {
        return;
    }

    // Transition to Idle
    state_ = ProcessorState::Idle;
    current_cstate_level_ = 0;

    if (engine_) {
        engine_->notify_processor_state_change(*this, ProcessorState::Sleep, ProcessorState::Idle);
    }

    // Fire ProcessorAvailable ISR
    if (on_processor_available_) {
        on_processor_available_(*this);
    }

    // Continue with normal assign flow (may trigger context switch)
    Job* saved_pending = pending_job_;
    pending_job_ = nullptr;

    if (engine_ && engine_->context_switch_enabled()) {
        Duration cs_delay = type_->context_switch_delay();
        if (cs_delay > Duration::zero()) {
            begin_context_switch(*saved_pending);
            return;
        }
    }

    // Direct transition to Running
    current_job_ = saved_pending;
    state_ = ProcessorState::Running;

    if (engine_) {
        last_update_time_ = engine_->time();
        schedule_completion();
    }
}

void Processor::begin_dvfs() {
    // Save state before DVFS
    pre_dvfs_state_ = state_;

    // If running, update consumed work and cancel completion timer
    if (state_ == ProcessorState::Running && current_job_) {
        update_consumed_work();
        cancel_completion();
    }

    state_ = ProcessorState::Changing;
}

void Processor::end_dvfs() {
    if (state_ != ProcessorState::Changing) {
        return;
    }

    // Check if clear() was called during DVFS
    if (pending_clear_) {
        pending_clear_ = false;

        // Cancel deadline timer if it was set
        if (engine_) {
            engine_->cancel_timer(deadline_timer_);
        }

        current_job_ = nullptr;
        state_ = ProcessorState::Idle;

        // Fire ProcessorAvailable ISR
        if (on_processor_available_) {
            on_processor_available_(*this);
        }
        return;
    }

    // Restore previous state
    state_ = pre_dvfs_state_;

    // If was Running, reschedule completion at new frequency
    if (state_ == ProcessorState::Running && current_job_) {
        reschedule_completion();
    }

    // Fire ProcessorAvailable ISR
    if (on_processor_available_) {
        on_processor_available_(*this);
    }
}

} // namespace schedsim::core
