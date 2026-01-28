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

// EventKey provides deterministic ordering for events (Decision 46)
// Order: time (primary) → priority (secondary) → sequence (tertiary)
struct EventKey {
    TimePoint time;      // Primary: simulation time
    int priority;        // Secondary: lower = fires first
    uint64_t sequence;   // Tertiary: insertion order (determinism)

    auto operator<=>(const EventKey&) const = default;
};

// Event priority constants (Decision 46)
// Lower values = higher priority (fire first at same time)
struct EventPriority {
    static constexpr int JOB_COMPLETION      = -400;
    static constexpr int DEADLINE_MISS       = -300;
    static constexpr int PROCESSOR_AVAILABLE = -200;
    static constexpr int JOB_ARRIVAL         = -100;
    static constexpr int TIMER_DEFAULT       = 0;
};

// Event types
struct JobArrivalEvent {
    Task* task;
    Duration exec_time;
};

struct JobCompletionEvent {
    Processor* proc;
    Job* job;
};

struct DeadlineMissEvent {
    Processor* proc;
    Job* job;
};

struct ProcessorAvailableEvent {
    Processor* proc;
};

struct TimerEvent {
    std::function<void()> callback;
};

// Event variant holding all possible event types
using Event = std::variant<
    JobArrivalEvent,
    JobCompletionEvent,
    DeadlineMissEvent,
    ProcessorAvailableEvent,
    TimerEvent
>;

} // namespace schedsim::core
