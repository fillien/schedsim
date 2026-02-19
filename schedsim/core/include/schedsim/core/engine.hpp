#pragma once

#include <schedsim/core/deferred.hpp>
#include <schedsim/core/event.hpp>
#include <schedsim/core/timer.hpp>
#include <schedsim/core/trace_writer.hpp>
#include <schedsim/core/types.hpp>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace schedsim::core {

class ClockDomain;
class EnergyTracker;
class Job;
class Platform;
class Processor;
class Task;
enum class ProcessorState;

/// @brief Event-driven simulation engine.
///
/// The Engine is the central simulation loop. It owns an event priority
/// queue and a Platform, and advances simulation time by dispatching
/// events in chronological order. Within a single timestep, events are
/// processed by priority, followed by all requested deferred callbacks.
///
/// The Engine is non-copyable and non-movable, designed for stack
/// allocation. A typical usage pattern is:
///
/// @code
/// core::Engine engine;
/// auto& plat = engine.platform();
/// // ... add hardware and tasks to platform ...
/// engine.finalize();
/// // ... set up scheduler ...
/// engine.run();
/// @endcode
///
/// @see Platform, TraceWriter
/// @ingroup core_engine
class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /// @brief Returns the current simulation time.
    [[nodiscard]] TimePoint time() const noexcept { return current_time_; }

    /// @brief Run simulation until the event queue is empty.
    void run();

    /// @brief Run simulation until the given time point.
    /// @param until Simulation stops after processing all events at this time.
    void run(TimePoint until);

    /// @brief Run simulation until the stop condition returns true.
    /// @param stop_condition Evaluated between timesteps; simulation stops when it returns true.
    void run(std::function<bool()> stop_condition);

    /// @brief Request the engine to stop after the current timestep completes.
    ///
    /// The current timestep finishes atomically; the flag is checked
    /// between timesteps. Auto-resets at the start of each run() call.
    void request_stop() noexcept;

    /// @brief Returns true if a stop has been requested.
    [[nodiscard]] bool stop_requested() const noexcept;

    /// @brief Schedule a one-shot timer.
    /// @param when Absolute simulation time to fire (must be > time()).
    /// @param priority Lower values fire first within the same timestep.
    /// @param callback Invoked when the timer fires.
    /// @return Identifier that can be passed to cancel_timer().
    /// @throws InvalidStateError if @p when <= time().
    TimerId add_timer(TimePoint when, int priority, std::function<void()> callback);

    /// @brief Schedule a one-shot timer with default priority.
    /// @param when Absolute simulation time to fire (must be > time()).
    /// @param callback Invoked when the timer fires.
    /// @return Identifier that can be passed to cancel_timer().
    TimerId add_timer(TimePoint when, std::function<void()> callback);

    /// @brief Cancel a pending timer.
    /// @param timer_id Timer to cancel; reset to invalid on return.
    void cancel_timer(TimerId& timer_id);

    /// @brief Register a deferred callback.
    ///
    /// Deferred callbacks execute at the end of the current timestep,
    /// after all events at that time have been processed. They are
    /// useful for batching rescheduling decisions.
    ///
    /// @param callback Function to invoke when requested.
    /// @return Identifier used with request_deferred().
    DeferredId register_deferred(std::function<void()> callback);

    /// @brief Request that a registered deferred callback fires this timestep.
    /// @param deferred_id Identifier returned by register_deferred().
    void request_deferred(DeferredId deferred_id);

    /// @brief Set the trace writer for simulation event logging.
    ///
    /// The Engine does not own the writer. Pass nullptr to disable tracing.
    /// @param writer Pointer to a TraceWriter, or nullptr.
    void set_trace_writer(TraceWriter* writer) noexcept;

    /// @brief Invoke a tracing callback only if a trace writer is set.
    ///
    /// Zero overhead when tracing is disabled: the callback is not
    /// invoked and no TraceRecord is allocated.
    ///
    /// @tparam F Callable with signature void(TraceWriter&).
    /// @param func Callback that writes trace data.
    template<typename F>
    void trace(F&& func);

    /// @brief Finalize the engine and its platform.
    ///
    /// Must be called after all hardware and tasks have been added to
    /// the Platform and before run(). Locks the platform collections
    /// and schedules initial job arrivals.
    void finalize();

    /// @brief Returns true if the engine has been finalized.
    [[nodiscard]] bool is_finalized() const noexcept { return finalized_; }

    /// @brief Insert a raw event into the event queue.
    /// @param when Simulation time for the event.
    /// @param priority Priority within the timestep (lower = earlier).
    /// @param event The event payload.
    void schedule_event(TimePoint when, int priority, Event event);

    /// @brief Access the simulation platform.
    [[nodiscard]] Platform& platform() noexcept;
    /// @brief Access the simulation platform (const).
    [[nodiscard]] const Platform& platform() const noexcept;

    /// @brief Schedule a job arrival event for a task.
    /// @param task The task that will receive the job.
    /// @param arrival_time When the job arrives.
    /// @param exec_time Actual execution time for this job instance.
    void schedule_job_arrival(Task& task, TimePoint arrival_time, Duration exec_time);

    /// @brief Callback type for handling job arrivals.
    using JobArrivalHandler = std::function<void(Task&, Job)>;

    /// @brief Set the global job arrival handler.
    ///
    /// Exactly one handler may be set per engine. Typically set by the
    /// scheduler to receive new jobs for admission and dispatch.
    ///
    /// @param handler Called on every job arrival event.
    /// @throws HandlerAlreadySetError if called more than once.
    void set_job_arrival_handler(JobArrivalHandler handler);

    /// @brief Enable or disable context switch overhead modeling.
    /// @param enabled True to enable, false to disable (default: disabled).
    void enable_context_switch(bool enabled) noexcept { context_switch_enabled_ = enabled; }

    /// @brief Returns true if context switch overhead is enabled.
    [[nodiscard]] bool context_switch_enabled() const noexcept { return context_switch_enabled_; }

    /// @brief Enable or disable energy tracking.
    ///
    /// Must be called before finalize(). When enabled, the engine
    /// tracks energy consumption per processor, clock domain, and
    /// power domain.
    ///
    /// @param enabled True to enable energy tracking.
    void enable_energy_tracking(bool enabled);

    /// @brief Returns true if energy tracking is enabled.
    [[nodiscard]] bool energy_tracking_enabled() const noexcept;

    /// @brief Returns cumulative energy consumed by a processor.
    /// @param proc_id Processor index (0-based).
    /// @throws InvalidStateError if energy tracking is disabled.
    [[nodiscard]] Energy processor_energy(std::size_t proc_id) const;

    /// @brief Returns cumulative energy consumed by a clock domain.
    /// @param cd_id Clock domain index (0-based).
    /// @throws InvalidStateError if energy tracking is disabled.
    [[nodiscard]] Energy clock_domain_energy(std::size_t cd_id) const;

    /// @brief Returns cumulative energy consumed by a power domain.
    /// @param pd_id Power domain index (0-based).
    /// @throws InvalidStateError if energy tracking is disabled.
    [[nodiscard]] Energy power_domain_energy(std::size_t pd_id) const;

    /// @brief Returns total energy consumed across all domains.
    /// @throws InvalidStateError if energy tracking is disabled.
    [[nodiscard]] Energy total_energy() const;

private:
    friend class ClockDomain;
    friend class Processor;

    struct DeferredCallback {
        std::function<void()> callback;
        bool requested{false};
    };

    void process_timestep();
    void dispatch_event(Event& event);
    void fire_deferred_callbacks();
    void emit_sim_finished();
    void notify_frequency_change(ClockDomain& cd, Frequency old_freq, Frequency new_freq);
    void notify_processor_state_change(Processor& proc, ProcessorState old_state,
                                       ProcessorState new_state);

    TimePoint current_time_{};
    uint64_t sequence_{0};
    bool finalized_{false};
    bool in_deferred_phase_{false};
    bool context_switch_enabled_{false};
    bool stop_requested_{false};

    std::map<EventKey, Event> event_queue_;
    std::vector<DeferredCallback> deferred_callbacks_;
    TraceWriter* trace_writer_{nullptr};

    std::unique_ptr<Platform> platform_;
    std::unique_ptr<EnergyTracker> energy_tracker_;
    JobArrivalHandler job_arrival_handler_;
};

// Template implementation
template<typename F>
void Engine::trace(F&& func) {
    if (trace_writer_) {
        trace_writer_->begin(current_time_);
        func(*trace_writer_);
        trace_writer_->end();
    }
}

} // namespace schedsim::core
