#pragma once

#include <schedsim/core/timer.hpp>
#include <schedsim/core/types.hpp>

#include <cstddef>
#include <functional>

namespace schedsim::core {

class ClockDomain;
class Engine;
class EnergyTracker;
class Job;
class Platform;
class PowerDomain;
class ProcessorType;
class Task;

/// @brief Enumeration of the possible states of a Processor.
///
/// Each processor is in exactly one state at any given simulation time.
/// State transitions are managed internally by Processor and triggered
/// by scheduling decisions, DVFS operations, and C-state requests.
enum class ProcessorState {
    Idle,              ///< No task assigned; the processor is available for scheduling.
    ContextSwitching,  ///< Transitioning to run a job (optional overhead modelling).
    Running,           ///< Actively executing a job.
    Sleep,             ///< In a low-power C-state (C1 or deeper).
    Changing           ///< A DVFS frequency transition is in progress.
};

/// @brief Models a single CPU core in the simulated platform.
/// @ingroup core_hardware
///
/// A Processor tracks its execution state and the job currently running on it.
/// It belongs to exactly one ClockDomain (shared frequency) and one PowerDomain
/// (shared C-state management). Scheduling decisions are communicated through
/// ISR-style callback handlers that the algo layer registers.
///
/// Processor is non-copyable but movable (Decision 61).
///
/// @see ClockDomain, PowerDomain, ProcessorType, Job
class Processor {
public:
    /// @brief Construct a Processor with its hardware affiliations.
    /// @param id         Unique processor identifier within the platform.
    /// @param type       Reference to the ProcessorType describing this core's
    ///                   micro-architecture (performance factor, context-switch
    ///                   overhead, etc.).
    /// @param clock_domain  The ClockDomain this processor belongs to.
    /// @param power_domain  The PowerDomain this processor belongs to.
    Processor(std::size_t id, ProcessorType& type,
              ClockDomain& clock_domain, PowerDomain& power_domain);

    /// @brief Return the unique identifier of this processor.
    /// @return Processor ID (zero-based index within the platform).
    [[nodiscard]] std::size_t id() const noexcept { return id_; }

    /// @brief Return the ProcessorType describing this core's micro-architecture.
    /// @return Reference to the associated ProcessorType.
    /// @see ProcessorType
    [[nodiscard]] ProcessorType& type() const noexcept { return *type_; }

    /// @brief Return the current execution state of the processor.
    /// @return The current ProcessorState.
    /// @see ProcessorState
    [[nodiscard]] ProcessorState state() const noexcept { return state_; }

    /// @brief Return the ClockDomain this processor belongs to.
    /// @return Reference to the associated ClockDomain.
    /// @see ClockDomain
    [[nodiscard]] ClockDomain& clock_domain() const noexcept { return *clock_domain_; }

    /// @brief Return the PowerDomain this processor belongs to.
    /// @return Reference to the associated PowerDomain.
    /// @see PowerDomain
    [[nodiscard]] PowerDomain& power_domain() const noexcept { return *power_domain_; }

    /// @brief Return the job currently assigned to this processor, if any.
    /// @return Pointer to the current Job, or nullptr if no job is assigned.
    /// @see Job
    [[nodiscard]] Job* current_job() const noexcept { return current_job_; }

    /// @brief Return the task that owns the current job, if any.
    /// @return Pointer to the current Task, or nullptr if no job is assigned.
    /// @see Task
    [[nodiscard]] Task* current_task() const noexcept;

    /// @brief Compute the normalised execution speed of this processor.
    ///
    /// Speed is defined as `(freq / freq_max) * (perf / reference_performance)`,
    /// where `freq` and `freq_max` come from the ClockDomain, `perf` from the
    /// ProcessorType, and `reference_performance` from the Platform.
    ///
    /// @param reference_performance  The platform-wide reference performance
    ///                               factor used for normalisation.
    /// @return Normalised speed in the range (0, 1] under typical configurations.
    [[nodiscard]] double speed(double reference_performance) const noexcept;

    /// @brief Return the current C-state level of this processor.
    /// @return C-state level: 0 means active (C0); higher values indicate
    ///         deeper sleep states.
    /// @see PowerDomain::c_states
    [[nodiscard]] int current_cstate_level() const noexcept { return current_cstate_level_; }

    /// @brief Assign a job to this processor for execution.
    ///
    /// If the processor is Idle, the job starts immediately (or after an
    /// optional context-switch delay). If the processor is in Sleep, a
    /// wake-up sequence is initiated; the job starts once the processor
    /// has woken up.
    ///
    /// @param job  The Job to execute on this processor.
    /// @throws InvalidStateError if the processor is not in Idle or Sleep state
    ///         (Decision 72).
    /// @see clear, Job
    void assign(Job& job);

    /// @brief Remove the current job from this processor.
    ///
    /// The exact behaviour depends on the current state:
    /// - **Running**: stops execution, returns to Idle.
    /// - **ContextSwitching**: cancels the transition; the job never runs.
    /// - **Changing** (DVFS in progress): sets a pending-clear flag; the
    ///   processor returns to Idle once the DVFS transition completes.
    ///
    /// @throws InvalidStateError if the processor is already Idle (Decision 53).
    /// @see assign
    void clear();

    /// @brief Request a transition to a low-power C-state.
    ///
    /// The processor enters the specified C-state level. Level 0 represents
    /// the fully active state (C0).
    ///
    /// @param level  The target C-state level (0 = C0/active, higher = deeper).
    /// @throws InvalidStateError if the processor is Running, Changing, or
    ///         ContextSwitching.
    /// @see PowerDomain, current_cstate_level
    void request_cstate(int level);

    /// @brief Callback type invoked when a job completes execution.
    /// @see set_job_completion_handler
    using JobCompletionHandler = std::function<void(Processor&, Job&)>;

    /// @brief Callback type invoked when a job misses its absolute deadline.
    /// @see set_deadline_miss_handler
    using DeadlineMissHandler = std::function<void(Processor&, Job&)>;

    /// @brief Callback type invoked when a processor becomes available.
    ///
    /// This fires after a DVFS transition completes and the processor has no
    /// pending work, or after a wake-up completes without a pending job.
    /// @see set_processor_available_handler
    using ProcessorAvailableHandler = std::function<void(Processor&)>;

    /// @brief Register a handler called when a job finishes its work.
    /// @param handler  The callback to invoke on job completion.
    /// @see JobCompletionHandler
    void set_job_completion_handler(JobCompletionHandler handler);

    /// @brief Register a handler called when a job's absolute deadline is missed.
    /// @param handler  The callback to invoke on deadline miss.
    /// @see DeadlineMissHandler
    void set_deadline_miss_handler(DeadlineMissHandler handler);

    /// @brief Register a handler called when the processor becomes available.
    /// @param handler  The callback to invoke when the processor is ready.
    /// @see ProcessorAvailableHandler
    void set_processor_available_handler(ProcessorAvailableHandler handler);

    // Non-copyable, movable (Decision 61)
    Processor(const Processor&) = delete;
    Processor& operator=(const Processor&) = delete;
    Processor(Processor&&) = default;
    Processor& operator=(Processor&&) = default;

private:
    friend class ClockDomain;
    friend class Engine;
    friend class EnergyTracker;
    friend class Platform;

    void set_engine(Engine* engine) noexcept { engine_ = engine; }
    void set_reference_performance(double ref_perf) noexcept { reference_performance_ = ref_perf; }

    // Internal event handling
    void on_completion_timer();
    void on_deadline_timer();

    // Context switch handling
    void begin_context_switch(Job& job);
    void on_context_switch_complete();

    // Wake-up handling (from Sleep state)
    void begin_wake_up(Job& job);
    void on_wake_up_complete();

    // DVFS handling (called by ClockDomain)
    void begin_dvfs();
    void end_dvfs();
    void notify_immediate_freq_change();

    // Execution tracking
    void schedule_completion();
    void cancel_completion();
    void update_consumed_work();
    void reschedule_completion();

    std::size_t id_;
    ProcessorType* type_;
    ClockDomain* clock_domain_;
    PowerDomain* power_domain_;
    ProcessorState state_{ProcessorState::Idle};
    Job* current_job_{nullptr};
    Engine* engine_{nullptr};
    double reference_performance_{1.0};

    // Transition state tracking
    Job* pending_job_{nullptr};           // Job waiting during CS/wake-up
    TimerId transition_timer_;             // Timer for CS/wake-up
    bool pending_clear_{false};            // clear() called during Changing
    int current_cstate_level_{0};          // 0 = active (C0)
    ProcessorState pre_dvfs_state_{ProcessorState::Idle};  // State before DVFS

    // ISR handlers
    JobCompletionHandler on_job_completion_;
    DeadlineMissHandler on_deadline_miss_;
    ProcessorAvailableHandler on_processor_available_;

    // Timer tracking
    TimerId completion_timer_;
    TimerId deadline_timer_;
    TimePoint last_update_time_;
};

} // namespace schedsim::core
