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

// Processor states (Decision 82)
enum class ProcessorState {
    Idle,              // No task, available
    ContextSwitching,  // Transitioning to run a job (optional overhead)
    Running,           // Executing a job
    Sleep,             // In a C-state
    Changing           // Frequency transition in progress
};

// Processor represents a single CPU core in the simulated platform.
// It tracks execution state and notifies Library 2 via ISR handlers.
class Processor {
public:
    Processor(std::size_t id, ProcessorType& type,
              ClockDomain& clock_domain, PowerDomain& power_domain);

    [[nodiscard]] std::size_t id() const noexcept { return id_; }
    [[nodiscard]] ProcessorType& type() const noexcept { return *type_; }
    [[nodiscard]] ProcessorState state() const noexcept { return state_; }
    [[nodiscard]] ClockDomain& clock_domain() const noexcept { return *clock_domain_; }
    [[nodiscard]] PowerDomain& power_domain() const noexcept { return *power_domain_; }
    [[nodiscard]] Job* current_job() const noexcept { return current_job_; }
    [[nodiscard]] Task* current_task() const noexcept;

    // Compute normalized processor speed: (freq/freq_max) * (perf/perf_ref)
    // Requires reference_performance from Platform.
    [[nodiscard]] double speed(double reference_performance) const noexcept;

    // Current C-state level (0 = active/C0, higher = deeper sleep)
    [[nodiscard]] int current_cstate_level() const noexcept { return current_cstate_level_; }

    // Job management (Decision 72)
    // assign() throws InvalidStateError if not Idle or Sleep
    // For Sleep: triggers wake-up sequence (async)
    void assign(Job& job);
    // clear() throws InvalidStateError if Idle (Decision 53)
    // During ContextSwitching: cancels transition, job never runs
    // During Changing: marks pending_clear_, returns to Idle after DVFS completes
    void clear();

    // C-state management
    // Throws InvalidStateError if Running, Changing, or ContextSwitching
    void request_cstate(int level);

    // ISR handlers (Decision 57)
    using JobCompletionHandler = std::function<void(Processor&, Job&)>;
    using DeadlineMissHandler = std::function<void(Processor&, Job&)>;
    using ProcessorAvailableHandler = std::function<void(Processor&)>;

    void set_job_completion_handler(JobCompletionHandler handler);
    void set_deadline_miss_handler(DeadlineMissHandler handler);
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
