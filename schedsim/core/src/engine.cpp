#include <schedsim/core/engine.hpp>
#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/energy_tracker.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/task.hpp>

#include <utility>

namespace schedsim::core {

Engine::Engine()
    : platform_(std::make_unique<Platform>(*this)) {}

Engine::~Engine() = default;

void Engine::run() {
    while (!event_queue_.empty()) {
        process_timestep();
    }
}

void Engine::run(TimePoint until) {
    while (!event_queue_.empty()) {
        // Check if next event is beyond our stop time
        auto it = event_queue_.begin();
        if (it->first.time > until) {
            current_time_ = until;
            return;
        }
        process_timestep();
    }
    // If queue becomes empty before reaching 'until', advance time
    if (current_time_ < until) {
        current_time_ = until;
    }
}

void Engine::run(std::function<bool()> stop_condition) {
    while (!event_queue_.empty() && !stop_condition()) {
        process_timestep();
    }
}

TimerId Engine::add_timer(TimePoint when, int priority, std::function<void()> callback) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot add timer after finalize()");
    }
    // Decision 7: Timers must be scheduled in future or at current time
    if (when < current_time_) {
        throw InvalidStateError("Timer must be scheduled in the future (when >= time())");
    }

    EventKey key{when, priority, sequence_++};
    Event event = TimerEvent{std::move(callback)};
    auto [it, inserted] = event_queue_.emplace(key, std::move(event));
    return TimerId(it);
}

TimerId Engine::add_timer(TimePoint when, std::function<void()> callback) {
    return add_timer(when, EventPriority::TIMER_DEFAULT, std::move(callback));
}

void Engine::cancel_timer(TimerId& timer_id) {
    if (!timer_id.valid_) {
        return; // No-op if already invalid
    }
    event_queue_.erase(timer_id.it_);
    timer_id.invalidate();
}

DeferredId Engine::register_deferred(std::function<void()> callback) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot register deferred callback after finalize()");
    }

    std::size_t index = deferred_callbacks_.size();
    deferred_callbacks_.push_back(DeferredCallback{std::move(callback), false});
    return DeferredId(index);
}

void Engine::request_deferred(DeferredId deferred_id) {
    if (!deferred_id.valid_) {
        return; // No-op if invalid
    }
    if (deferred_id.index_ >= deferred_callbacks_.size()) {
        return; // Invalid index
    }
    deferred_callbacks_[deferred_id.index_].requested = true;
}

void Engine::set_trace_writer(TraceWriter* writer) noexcept {
    trace_writer_ = writer;
}

void Engine::finalize() {
    platform_->finalize();
    finalized_ = true;
}

Platform& Engine::platform() noexcept {
    return *platform_;
}

const Platform& Engine::platform() const noexcept {
    return *platform_;
}

void Engine::schedule_job_arrival(Task& task, TimePoint arrival_time, Duration exec_time) {
    if (arrival_time < current_time_) {
        throw InvalidStateError("Cannot schedule job arrival in the past");
    }

    schedule_event(arrival_time, EventPriority::JOB_ARRIVAL,
                   JobArrivalEvent{&task, exec_time});
}

void Engine::set_job_arrival_handler(JobArrivalHandler handler) {
    if (job_arrival_handler_) {
        throw HandlerAlreadySetError("Job arrival handler already set");
    }
    job_arrival_handler_ = std::move(handler);
}

void Engine::schedule_event(TimePoint when, int priority, Event event) {
    if (finalized_) {
        throw AlreadyFinalizedError("Cannot schedule event after finalize()");
    }
    if (when < current_time_) {
        throw InvalidStateError("Cannot schedule event in the past");
    }

    EventKey key{when, priority, sequence_++};
    event_queue_.emplace(key, std::move(event));
}

void Engine::process_timestep() {
    if (event_queue_.empty()) {
        return;
    }

    // Get the current timestep (time of first event)
    TimePoint timestep = event_queue_.begin()->first.time;
    current_time_ = timestep;

    // Process all events at this timestep
    while (!event_queue_.empty()) {
        auto it = event_queue_.begin();
        if (it->first.time != timestep) {
            break; // Done with this timestep
        }

        Event event = std::move(it->second);
        event_queue_.erase(it);

        dispatch_event(event);
    }

    // Fire deferred callbacks after all events at this timestep
    fire_deferred_callbacks();
}

void Engine::dispatch_event(Event& event) {
    std::visit([this](auto& ev) {
        using T = std::decay_t<decltype(ev)>;
        if constexpr (std::is_same_v<T, JobArrivalEvent>) {
            // Create Job, transfer ownership to Library 2 via handler
            if (ev.task != nullptr && job_arrival_handler_) {
                Job job{*ev.task, ev.exec_time,
                        current_time_ + ev.task->relative_deadline()};
                job_arrival_handler_(*ev.task, std::move(job));
            }
        } else if constexpr (std::is_same_v<T, TimerEvent>) {
            if (ev.callback) {
                ev.callback();
            }
        }
        // JobCompletionEvent, DeadlineMissEvent, ProcessorAvailableEvent
        // are handled directly by Processor's timer callbacks, not through
        // the event queue. The events are defined in event.hpp for
        // completeness but dispatched via Timer callbacks.
    }, event);
}

void Engine::fire_deferred_callbacks() {
    in_deferred_phase_ = true;

    // Decision 55: Single-pass semantics
    // Callbacks requested during this phase will fire in the next batch
    // We iterate by index to handle requests during iteration
    for (std::size_t i = 0; i < deferred_callbacks_.size(); ++i) {
        if (deferred_callbacks_[i].requested) {
            deferred_callbacks_[i].requested = false;
            if (deferred_callbacks_[i].callback) {
                deferred_callbacks_[i].callback();
            }
        }
    }

    in_deferred_phase_ = false;
}

void Engine::enable_energy_tracking(bool enabled) {
    if (enabled && !energy_tracker_) {
        energy_tracker_ = std::make_unique<EnergyTracker>(*platform_, current_time_);
    } else if (!enabled) {
        energy_tracker_.reset();
    }
}

bool Engine::energy_tracking_enabled() const noexcept {
    return energy_tracker_ != nullptr;
}

Energy Engine::processor_energy(std::size_t proc_id) const {
    if (!energy_tracker_) {
        throw InvalidStateError("Energy tracking is not enabled");
    }
    energy_tracker_->update_to_time(current_time_);
    return energy_tracker_->processor_energy(proc_id);
}

Energy Engine::clock_domain_energy(std::size_t cd_id) const {
    if (!energy_tracker_) {
        throw InvalidStateError("Energy tracking is not enabled");
    }
    energy_tracker_->update_to_time(current_time_);
    return energy_tracker_->clock_domain_energy(cd_id);
}

Energy Engine::power_domain_energy(std::size_t pd_id) const {
    if (!energy_tracker_) {
        throw InvalidStateError("Energy tracking is not enabled");
    }
    energy_tracker_->update_to_time(current_time_);
    return energy_tracker_->power_domain_energy(pd_id);
}

Energy Engine::total_energy() const {
    if (!energy_tracker_) {
        throw InvalidStateError("Energy tracking is not enabled");
    }
    energy_tracker_->update_to_time(current_time_);
    return energy_tracker_->total_energy();
}

void Engine::notify_frequency_change(ClockDomain& cd, Frequency old_freq, Frequency new_freq) {
    if (energy_tracker_) {
        energy_tracker_->on_frequency_change(cd, old_freq, new_freq, current_time_);
    }

    trace([&](TraceWriter& w) {
        w.type("frequency_change");
        w.field("clock_domain_id", static_cast<uint64_t>(cd.id()));
        w.field("old_freq_mhz", old_freq.mhz);
        w.field("new_freq_mhz", new_freq.mhz);
    });
}

} // namespace schedsim::core
