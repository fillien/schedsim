#pragma once

#include <schedsim/core/types.hpp>

#include <compare>
#include <cstdint>
#include <functional>
#include <variant>

namespace schedsim::core {

// Forward declarations for Phase 2+
class Task;
class Processor;
class Job;

/// @brief Deterministic ordering key for events in the priority queue.
///
/// Events are ordered first by simulation time, then by priority
/// (lower values fire first), then by insertion sequence number to
/// guarantee determinism when time and priority are equal.
///
/// @see EventPriority, Engine::schedule_event
/// @ingroup core_events
struct EventKey {
    TimePoint time;      ///< Primary: simulation time at which the event fires.
    int priority;        ///< Secondary: lower values fire first within a timestep.
    uint64_t sequence;   ///< Tertiary: insertion order for determinism.

    /// @cond INTERNAL
    auto operator<=>(const EventKey&) const = default;
    /// @endcond
};

/// @brief Named constants for event dispatch priority.
///
/// Lower numeric values indicate higher priority (fire first within the
/// same simulation timestep). The ordering ensures that completions are
/// processed before deadline checks, which are processed before new
/// arrivals, matching standard real-time scheduling semantics.
///
/// @see EventKey, Engine::add_timer
/// @ingroup core_events
struct EventPriority {
    static constexpr int JOB_COMPLETION      = -400; ///< Highest: job finishes executing.
    static constexpr int DEADLINE_MISS       = -300; ///< Deadline miss detection.
    static constexpr int PROCESSOR_AVAILABLE = -200; ///< Processor becomes idle or wakes.
    static constexpr int JOB_ARRIVAL         = -100; ///< New job arrives for scheduling.
    static constexpr int TIMER_DEFAULT       = 0;    ///< Default priority for user timers.
};

/// @brief A new job instance arrives for a task.
///
/// Carries the task that generated the job and the actual execution
/// time for this particular instance (may be less than the WCET).
///
/// @see Engine::schedule_job_arrival, Engine::set_job_arrival_handler
/// @ingroup core_events
struct JobArrivalEvent {
    Task* task;          ///< Task that generated this job.
    Duration exec_time;  ///< Actual execution time for this job instance.
};

/// @brief A job has finished executing on its processor.
///
/// Fired when the processor completes all remaining work for the job.
/// The scheduler should release the processor and update its ready queue.
///
/// @see Processor, Job
/// @ingroup core_events
struct JobCompletionEvent {
    Processor* proc;  ///< Processor that completed the job.
    Job* job;          ///< Job that finished execution.
};

/// @brief A job's absolute deadline has been reached.
///
/// Fired when a job's deadline expires. The scheduler's deadline miss
/// policy determines whether the simulation continues or aborts.
///
/// @see Job, Processor
/// @ingroup core_events
struct DeadlineMissEvent {
    Processor* proc;  ///< Processor running the job (may be nullptr if queued).
    Job* job;          ///< Job whose deadline has been missed.
};

/// @brief A processor has become available for scheduling.
///
/// Fired when a processor transitions to an idle state (e.g., after
/// completing a job or waking from a low-power C-state). The scheduler
/// should check its ready queue for pending work.
///
/// @see Processor
/// @ingroup core_events
struct ProcessorAvailableEvent {
    Processor* proc;  ///< Processor that became available.
};

/// @brief A one-shot timer callback fires.
///
/// Created by Engine::add_timer(). The callback is invoked during
/// event dispatch at the scheduled time and priority.
///
/// @see Engine::add_timer, Engine::cancel_timer
/// @ingroup core_events
struct TimerEvent {
    std::function<void()> callback;  ///< User-provided callback to invoke.
};

/// @brief Variant holding all possible event types in the simulation.
///
/// The Engine dispatches events by visiting this variant. Each
/// alternative corresponds to a distinct simulation occurrence.
///
/// @see EventKey, Engine
/// @ingroup core_events
using Event = std::variant<
    JobArrivalEvent,
    JobCompletionEvent,
    DeadlineMissEvent,
    ProcessorAvailableEvent,
    TimerEvent
>;

} // namespace schedsim::core
