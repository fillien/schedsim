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
class Task;

// Engine is the simulation event loop (Decisions 66, 68)
// Non-copyable, non-movable, designed for stack allocation
class Engine {
public:
    Engine();
    ~Engine();

    // Non-copyable, non-movable (Decision 68)
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Current simulation time
    [[nodiscard]] TimePoint time() const noexcept { return current_time_; }

    // Run simulation (Decision 66: three overloads)
    void run();                                      // Stop when queue empty
    void run(TimePoint until);                       // Stop at given time
    void run(std::function<bool()> stop_condition); // Stop when callback returns true

    // Timer API (Decisions 5, 7, 52)
    // Throws InvalidStateError if when <= time() (Decision 7)
    // Throws AlreadyFinalizedError if finalized
    TimerId add_timer(TimePoint when, int priority, std::function<void()> callback);
    TimerId add_timer(TimePoint when, std::function<void()> callback);
    void cancel_timer(TimerId& timer_id);

    // Deferred callback API (Decisions 10, 55, 87)
    // Throws AlreadyFinalizedError if finalized
    DeferredId register_deferred(std::function<void()> callback);
    void request_deferred(DeferredId deferred_id);

    // Trace API (Decisions 35, 80)
    void set_trace_writer(TraceWriter* writer) noexcept;

    // Zero-overhead trace: callback only invoked if writer is set
    template<typename F>
    void trace(F&& func);

    // Finalization API (Decision 47)
    void finalize();
    [[nodiscard]] bool is_finalized() const noexcept { return finalized_; }

    // Event queue access for internal use
    // (Will be used by Library 2 for scheduling events)
    void schedule_event(TimePoint when, int priority, Event event);

    // Platform access
    [[nodiscard]] Platform& platform() noexcept;
    [[nodiscard]] const Platform& platform() const noexcept;

    // Job arrival scheduling (Decision 56)
    // Schedules a job arrival event at the specified time
    void schedule_job_arrival(Task& task, TimePoint arrival_time, Duration exec_time);

    // Job arrival handler (Decision 60: one per engine)
    // Throws HandlerAlreadySetError if called twice
    using JobArrivalHandler = std::function<void(Task&, Job)>;
    void set_job_arrival_handler(JobArrivalHandler handler);

    // Feature flags (Phase 3 dynamic behaviors)
    // Context switch overhead: disabled by default for backward compatibility
    void enable_context_switch(bool enabled) noexcept { context_switch_enabled_ = enabled; }
    [[nodiscard]] bool context_switch_enabled() const noexcept { return context_switch_enabled_; }

    // Energy tracking: disabled by default for performance
    // Must be enabled before finalize() to set up tracking
    void enable_energy_tracking(bool enabled);
    [[nodiscard]] bool energy_tracking_enabled() const noexcept;

    // Energy queries (throw InvalidStateError if tracking disabled)
    [[nodiscard]] Energy processor_energy(std::size_t proc_id) const;
    [[nodiscard]] Energy clock_domain_energy(std::size_t cd_id) const;
    [[nodiscard]] Energy power_domain_energy(std::size_t pd_id) const;
    [[nodiscard]] Energy total_energy() const;

private:
    friend class ClockDomain;

    struct DeferredCallback {
        std::function<void()> callback;
        bool requested{false};
    };

    void process_timestep();
    void dispatch_event(Event& event);
    void fire_deferred_callbacks();
    void notify_frequency_change(ClockDomain& cd, Frequency old_freq, Frequency new_freq);

    TimePoint current_time_{Duration{0.0}};
    uint64_t sequence_{0};
    bool finalized_{false};
    bool in_deferred_phase_{false};
    bool context_switch_enabled_{false};

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
