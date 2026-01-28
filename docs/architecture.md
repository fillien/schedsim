# Architecture Design: Three-Library Simulator

## Motivation

The current simulator has a deep class hierarchy where core simulation
concepts (tasks, processors, platforms) are tangled with scheduling algorithms
(CBS, GRUB, EDF) and platform policies (DVFS, DPM). This makes it difficult
to:
- Implement new scheduling algorithms without understanding the entire codebase
- Reuse the core simulation engine for non-CBS scheduling models
- Compose algorithm features independently

The goal is to split the simulator into three statically linked libraries with
clear boundaries.

## Three-Library Overview

| Library | Name | Role |
|---|---|---|
| **Library 1** | `libschedsim-core` | Simulation engine, hardware platform model, execution tracking. The "physics." |
| **Library 2** | `libschedsim-algo` | Scheduling algorithms (CBS, GRUB, EDF), DVFS/DPM policies, allocators. The "intelligence." |
| **Library 3** | `libschedsim-io` | Configuration loading (JSON platform/scenario), result output (JSON, socket/DB). The "I/O." |

Dependencies: Library 2 → Library 1. Library 3 → Library 1 + Library 2.
Library 1 has no dependencies on Library 2 or Library 3.

Researchers write C++ (or Python through wrappers) applications that link
against these libraries.

---

## Library 1: Core (`libschedsim-core`)

### Responsibility

Library 1 models the simulated hardware platform and simulation engine. It
provides the "physics" of the simulation: tasks have jobs that consume CPU
time, processors run tasks at some frequency, and the engine advances
simulation time.

Library 1 knows nothing about CBS, GRUB, EDF, or any scheduling algorithm.

### Contents

- **Engine**: Discrete-event simulation loop, time management, event queue,
  ISR dispatch, deferred reschedule
- **Task**: A recurring computation. Has a period, utilization, deadline
  parameters. Receives jobs.
- **Job**: A request belonging to a task. Has a work quantity in reference
  units. Belongs to exactly one task. Queue management is Library 2.
- **Processor**: The minimal processing unit. Can run one task at a time.
  Belongs to exactly one clock domain and one power domain.
- **Processor Type**: Defines performance characteristics. Each processor
  belongs to a type.
- **Clock Domain**: Group of processors sharing a frequency source.
- **Power Domain (C-state Domain)**: Group of processors sharing C-state
  levels.
- **Platform**: Collection of processors, clock domains, power domains,
  and processor types.
- **Timer**: Programmable one-shot timers using closures.
- **Energy Tracker**: Optional power/energy consumption tracking during
  simulation.

### Engine Main Loop

```
engine.run(until):
    finalize_if_needed()

    while event_queue_ is not empty:
        front = event_queue_.begin()
        if front.key.time > until: break

        current_time_ = front.key.time

        // Phase 1: dispatch all events at current_time_
        while event_queue_ is not empty
              AND event_queue_.begin().key.time == current_time_:
            [key, event] = extract event_queue_.begin()
            std::visit(dispatch event → call ISR handler or timer closure)

        // Phase 2: single-pass deferred callbacks
        for each deferred in deferred_callbacks_ (registration order):
            if deferred.requested:
                deferred.requested = false
                deferred.callback()
        // Note: if a deferred callback calls request_deferred() for
        // another callback, it does NOT fire in this pass.
```

Key properties:
- All events at the same `TimePoint` are dispatched before any deferred
  callback fires. This guarantees the scheduler sees all simultaneous
  events before rescheduling.
- Within a timestep, events are ordered by priority (from `EventKey`).
- Deferred callbacks fire exactly once per timestep (single pass).
- Deferred callbacks requesting other deferred callbacks: deferred in
  the next batch only.

### No Cluster Concept

The old "Cluster" abstraction is removed. It conflated three independent
hardware concerns:
1. A group of processors (now: just a list of processors)
2. A shared frequency (now: Clock Domain)
3. Shared power management (now: Power Domain)

On real hardware, these groupings do not always coincide. Separating them
allows accurate modeling of diverse platforms.

### Clock Domains

A clock domain groups processors that share a frequency source. When a
frequency change is requested, all processors in that domain transition.

```
ClockDomain:
  processors() -> [Processor]
  frequency() -> Frequency
  set_frequency(freq)           // Affects all processors in this domain
  freq_max() -> Frequency
  freq_min() -> Frequency
```

During a frequency transition, processors enter "Changing" state. When the
transition completes, Library 1 fires the Processor Available ISR for each
affected processor.

### Power Domains and C-States

A power domain groups processors that share deep sleep states. Each power
domain defines a list of C-state levels, ordered by depth:

```
PowerDomain:
  processors() -> [Processor]
  c_states() -> [CStateLevel]

CStateLevel:
  level() -> int
  scope() -> PerProcessor | DomainWide
  wake_latency() -> Time
  power() -> Power
```

**C-state negotiation:**
- Library 2 requests a C-state level for a processor: `proc.request_cstate(level)`
- Per-processor C-states: applied immediately
- Domain-wide C-states: Library 1 checks if ALL processors in the domain
  have requested at least this level. The achieved C-state is the deepest
  level where all constraints are met.
- Wake-up: Library 1 applies the wake-up latency and fires the Processor
  Available ISR when the processor is ready.

**Example platform:**
```
Power domain A (processors P0-P3):
  C1: per_processor,  latency=1us,   power=50mW
  C3: domain_wide,    latency=100us, power=5mW

Power domain B (processors P4-P7):
  C1: per_processor,  latency=1us,   power=80mW
  C2: per_processor,  latency=10us,  power=20mW
  C3: domain_wide,    latency=200us, power=10mW
```

### Processor State Machine

```
                assign(job)                          clear()
    Idle ─────────────────► [ContextSwitching] ─────► Running ──────► Idle
     │                                                   │              ▲
     │ request_cstate()              set_frequency()     │              │
     ▼                                                   ▼              │
    Sleep                                           Changing ──────────┘
```

States:
- **Idle**: no task, processor available. ISR handlers can be called.
- **ContextSwitching**: (optional, if context switch enabled) processor
  transitioning to run a job. `assign()` throws. After the configured
  delay, transitions to Running. Processor Available ISR fires.
- **Running**: job assigned, consuming execution time.
- **Sleep**: in a C-state. `assign()` throws. Wake-up applies latency,
  then fires Processor Available ISR.
- **Changing**: frequency transition in progress. `assign()` throws.
  When transition completes, fires Processor Available ISR. If the
  processor had a running task, execution is paused during transition.

Transitions:
- `Idle → Running` (no CS overhead): via `proc.assign(job)`.
- `Idle → ContextSwitching → Running` (CS enabled): via
  `proc.assign(job)`. Processor Available ISR fires on completion.
- `Running → Idle`: via `proc.clear()`. Cancels pending Job Completion.
- `Idle → Sleep`: via `proc.request_cstate(level)`.
- `Sleep → Idle`: Library 1 wake-up (latency timer fires Processor
  Available ISR).
- `Running → Changing`: via `clock_domain.set_frequency()`. Task remains
  assigned but execution is paused during transition.
- `Changing → Running`: transition complete. Execution resumes at new
  frequency. Processor Available ISR fires.
- `Changing → Idle`: if task was cleared during transition.

Error cases (throw):
- `assign()` on Sleep, Changing, ContextSwitching, or Running
- `clear()` on Idle (no task to clear)
- `request_cstate()` on Running, Changing, or ContextSwitching

### Default Platform State

The platform is initialized at **maximum computing power**: maximum frequency
on all clock domains, all processors active (no C-state). Schedulers may
reduce performance as their policies dictate. If no DVFS/DPM policy is
installed, the platform stays at maximum performance.

### Processor Types and Per-Type Execution Times

Library 1 has a first-class concept of **processor type**. Each processor
belongs to a type. Processors of the same type have the same performance
characteristics.

Each task specifies a single WCET in **reference units** (wall time on the
reference processor at maximum frequency). When a task is assigned to a
processor, Library 1 derives the wall time from the reference WCET and the
processor's speed.

The **reference processor type** is the one with the highest speed factor.
All work quantities (job execution times, WCETs) are expressed in
**reference units** = wall time on the reference processor at maximum
frequency. Speed is normalized: `speed = (freq/freq_max) * (perf/perf_ref)`.
Reference at max freq has speed = 1.0.

Task utilization: `U = wcet / period`.
Per-type wall times: `wall_type = wcet / speed_type`.

```
Processor types:
  LITTLE: performance = 1.0
  big:    performance = 2.5  (reference — highest)

Normalized speeds (at max frequency):
  big:    speed = 2.5/2.5 = 1.0
  LITTLE: speed = 1.0/2.5 = 0.4

Task T1:
  period: 10
  wcet: 2.0  (reference units = wall time on big at max freq)
  Wall time by type:
    big:    2.0 / 1.0 = 2.0
    LITTLE: 2.0 / 0.4 = 5.0
  U = 2.0 / 10 = 0.2
```

For homogeneous platforms, there is only one processor type (perf = 1.0)
and reference units equal wall time (speed = 1.0).

### Time Management

All time-related computation (scaling, conversion, arithmetic, rounding) is
handled exclusively by Library 1.

Time is represented using a custom `SimClock` with `std::chrono`:
```cpp
struct SimClock {
    using rep        = double;
    using period     = std::ratio<1>;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<SimClock>;
    static constexpr bool is_steady = true;
};

using Duration  = SimClock::duration;     // relative intervals
using TimePoint = SimClock::time_point;   // absolute from epoch 0
```

This provides:
- **Type safety**: `TimePoint` and `Duration` are distinct types.
  `TimePoint + TimePoint` is a compile error. `TimePoint - TimePoint`
  yields `Duration`. `TimePoint + Duration` yields `TimePoint`.
- **Arithmetic for free**: `+`, `-`, comparisons from `<chrono>`
- **Dimensional correctness for CBS**: `Duration / Duration = double`
  (utilization), `double * Duration = Duration` (budget computation)
- **Future migration path**: changing `rep` from `double` to `int64_t`
  and `period` to `std::nano` switches to discrete nanosecond time.
  Library 2 code using `Duration`/`TimePoint` adapts automatically.
- **Numerical stability**: epsilon/rounding concerns centralized in Library 1

Library 2 uses `Duration` and `TimePoint` opaquely. It never does raw
floating-point arithmetic on time values.

### ISR Model (Interrupt Service Routines)

Library 1 provides an ISR mechanism modeled after real processor interrupts.
Each ISR vector represents a core event the simulated platform can generate.
Library 2 installs handlers (callbacks) for the vectors it cares about.

#### Core ISR Vectors

| ISR Vector | Trigger | Data provided |
|---|---|---|
| **Job Arrival** | New job arrives for a task (from scenario) | Task ref, job exec time |
| **Job Completion** | A job consumed all its execution time | Task ref, processor ref |
| **Deadline Miss** | A job was not completed by its deadline | Task ref, job ref |
| **Processor Available** | Processor ready after DVFS or C-state wake-up | Processor ref |

**Job Completion** is fired by Library 1 because Library 1 tracks execution
progress. When a task runs on a processor, Library 1 decrements the remaining
execution time based on processor speed (frequency x performance factor). When
it reaches zero, Library 1 fires the ISR. If the task is removed before
completion, Library 1 cancels the pending completion event.

**Deadline Miss** is fired by Library 1. Library 1 sets an internal timer at
the job's absolute deadline. If the job is still pending when the deadline
arrives, Library 1 fires this ISR. This allows Library 2 to react at runtime
(abort job, lower QoS, log the miss).

**Processor Available** is fired when:
- A frequency change completes (processor transitions from "Changing")
- A processor wakes from C-state and is ready to execute
- Context switch delay completes (if enabled)

#### Timer Callbacks (Closures)

Library 2 schedules one-shot timers with closures:

```cpp
auto id = engine.add_timer(
    exhaust_time,
    [this, server]() { on_budget_exhausted(server); }
);
```

Timers must be scheduled **strictly in the future** (`time > engine.time()`).
No same-time timer events. If Library 2 needs an immediate effect, it acts
directly in the handler.

Timers and core ISRs share the same event queue. Within a time step, events
are ordered by **configurable priority**:
1. Job Completion (highest)
2. Deadline Miss
3. Processor Available
4. Timer callbacks (configurable priority among themselves)
5. Job Arrival (lowest)

This ordering is configurable by the researcher.

#### ISR Routing

ISR handlers are registered **per processor** for Job Completion, Deadline
Miss, and Processor Available.

**Job Arrival** is global: it always goes to the **allocator**. The allocator
(Library 2) decides which scheduler handles the task. All job arrivals go
through the allocator, including recurring arrivals for already-assigned tasks,
to give the researcher the ability to do per-job reallocation.

#### Deferred Callbacks ("Last Event" Callbacks)

ISR handlers should not reschedule directly. Instead, Library 1 provides a
**deferred callback** mechanism: callbacks guaranteed to execute after all
events of the current time step.

```cpp
// Library 2: scheduler registers a deferred callback at setup
auto resched_id = engine.register_deferred(
    [this]() { on_resched(); }
);

// Library 2: ISR handler flags the deferred callback
void on_job_completion(Task& task, Processor& proc) {
    // ... update CBS state ...
    engine.request_deferred(resched_id);
}
```

Semantics:
- `register_deferred(closure) -> DeferredId` — register a callback
- `request_deferred(DeferredId)` — flag it for execution after current
  time step
- After ALL events at time T are processed, Library 1 calls all flagged
  deferred callbacks
- Each callback fires **at most once** per time step (Library 1 deduplicates)
- Multiple ISR handlers can flag the same deferred callback; it still fires
  once
- **Ordering**: deterministic, by registration order. Schedulers register
  in the order they are constructed, which follows processor declaration
  order.

This is more general than a resched-specific mechanism. Library 2 can use
deferred callbacks for any post-event processing (rescheduling, utilization
updates, trace flushing, etc.).

### Job Queue and Overrun Policy

A task can receive a new job before its previous job completes (overrun).
Job objects are created by Library 1 (core data), but the **job queue is
managed by Library 2**. Library 2 decides the overrun policy per-server:
- **Queue**: new job is queued, processed after current job completes
- **Skip**: new job is dropped silently
- **Abort**: current job is aborted, new job starts

Library 1 fires the Job Arrival ISR for every incoming job. Library 2
(via the allocator and scheduler) decides what to do with it. Library 1
does not auto-advance to the next job on completion — Library 2 explicitly
manages job lifecycle.

### Context Switch Overhead

Context switch overhead can be optionally enabled. When enabled:
- `proc.assign(job)`: the processor enters `ContextSwitching` state for
  a configured delay (per processor type). After the delay, the
  processor transitions to Running and execution begins. The Processor
  Available ISR fires when switching completes.
- `proc.clear()`: immediate (no overhead on clear for now).
- During ContextSwitching, no execution happens (job doesn't consume work).

This is a Library 1 mechanism (hardware characteristic). Enabled/disabled via
engine configuration. When disabled, `assign()` transitions directly to
Running (no delay).

### Execution Tracking

Library 1 is responsible for tracking job execution in **reference units**
(wall time on reference processor at maximum frequency). Speed is
normalized: `speed = (freq / freq_max) * (perf / perf_ref)`.

- **Assign**: `proc.assign(task)` — Library 1 reads the job's remaining
  time (reference units), computes speed, schedules a
  `JobCompletionEvent` at `now + remaining / speed`
- **Clear**: `proc.clear()` — Library 1 computes consumed time since
  last update (`wall_elapsed * speed`), updates remaining, cancels
  pending completion event
- **DVFS change**: Library 1 updates consumed time, recomputes completion
  at new speed
- **Migration**: remaining time unchanged (reference units). New speed
  reflects the new processor type. Completion adjusts automatically.
- **Completion**: remaining reaches zero → Library 1 fires the Job
  Completion ISR. Library 1 does NOT auto-advance the job queue.

Library 2 never computes job completion times. It only manages
algorithm-specific timers (e.g., CBS budget exhaustion).

**Migration example:**
```
big (perf=2.5, reference), LITTLE (perf=1.0). Job remaining = 2.0.
  t=0:   assign to big.    speed=1.0. completion at t=2.0
  t=0.8: preempt on big.   consumed=0.8*1.0=0.8. remaining=1.2
  t=0.8: assign to LITTLE. speed=0.4. completion at t=0.8+3.0=3.8
  t=3.8: job completes.    consumed=3.0*0.4=1.2. total=2.0 ✓
```

### Energy Tracking

Library 1 optionally tracks power and energy consumption during simulation.
When enabled:
- Library 1 integrates power over time for each processor, based on:
  - Current frequency (from clock domain)
  - Current C-state (from power domain)
  - Active vs idle state
  - Power model parameters from platform configuration
- Cumulative energy is available per processor, per clock domain, per power
  domain, and platform-wide
- Power model: `P = P_static(c_state) + P_dynamic(frequency, active)`

Energy tracking is **disabled by default** for performance. Enabled via engine
configuration.

### Simulation Termination

`engine.run()` supports three termination modes:
- **Fixed duration**: `engine.run(until=1000.0)` — stop at a given time
- **Empty queue**: `engine.run()` — stop when no more events are pending
- **Condition**: `engine.run(stop_when=callback)` — stop when the callback
  returns true (checked after each time step)

### Trace Sink (Writer-Callback Approach)

Library 1 provides a trace system using a **writer-callback** pattern. Each
trace emission passes a lambda that writes its fields to a `TraceWriter`
interface. Library 3 provides concrete writers.

```cpp
// Library 1 defines the writer interface
class TraceWriter {
    virtual void begin(TimePoint time) = 0;    // Start a trace record
    virtual void type(std::string_view name) = 0;
    virtual void field(std::string_view key, double value) = 0;
    virtual void field(std::string_view key, uint64_t value) = 0;
    virtual void field(std::string_view key, std::string_view value) = 0;
    virtual void end() = 0;                    // Finish the trace record
};

// Emission: both Library 1 and Library 2 use the same mechanism
engine.trace([&](TraceWriter& w) {
    w.type("job_arrival");
    w.field("tid", task.id());
    w.field("duration", duration);
});

// Library 2 emits algorithm-specific traces the same way
engine.trace([&](TraceWriter& w) {
    w.type("serv_budget_exhausted");
    w.field("sid", server.id());
    w.field("tid", server.task().id());
});
```

**Performance**: No heap allocation per trace. When the sink is null (no
tracing), `engine.trace()` is a single branch (check if writer is set).
The writer lambda is never called if no sink is configured.

**Usability**: New trace types require no registration. Library 2 (or
researcher code) simply calls `engine.trace()` with the appropriate fields.

Library 3 provides concrete `TraceWriter` implementations:
- **JsonTraceWriter**: streams JSON to file
- **MemoryTraceWriter**: buffers structured data in memory for post-processing
- **NullTraceWriter**: discards all traces (maximum performance)
- **SocketTraceWriter**: sends traces over network (future work)

The trace sink can be **null** (no tracing) for maximum performance.

### Library 1 API (Draft)

```
Types:
  SimClock                                     // Custom chrono clock
  Duration   = SimClock::duration              // Relative intervals
  TimePoint  = SimClock::time_point            // Absolute time (epoch = 0)
  Frequency  { double mhz; }                  // Strong type
  Power      { double mw;  }                  // Strong type
  Energy     { double mj;  }                  // Strong type

Engine (non-movable, non-copyable, stack-allocated):
  time() -> TimePoint                          // Current simulation time
  run()                                        // Run until queue empty
  run(TimePoint until)                         // Run until time
  run(std::function<bool()> stop)              // Run until condition
  add_timer(TimePoint, int priority, closure) -> TimerId   // Library 2 timers
  cancel_timer(TimerId)                        // Cancel; no-op if already fired
  register_deferred(closure) -> DeferredId     // Register last-event callback
  request_deferred(DeferredId)                 // Flag for post-event execution
  trace(writer_lambda)                         // Emit a trace event
  set_trace_writer(TraceWriter*)               // Set trace output (null=off)
  schedule_job_arrival(Task&, TimePoint, Duration) // Schedule a job arrival
  set_job_arrival_handler(handler)             // One per engine; throws if set twice
  enable_energy_tracking(bool)                 // Enable/disable energy
  enable_context_switch(bool)                  // Enable/disable CS overhead
  // Internal (private): add_internal_event()  // Reserved priorities for ISRs

Platform:
  add_processor_type(name, perf) -> ProcessorType&
  add_processor(type, clock_domain, power_domain) -> Processor&
  add_clock_domain(freq_range) -> ClockDomain&
  add_power_domain(c_states) -> PowerDomain&
  finalize()                                   // Lock collections; auto by run()
  processors() -> span<Processor>              // In declaration order
  processor_types() -> span<ProcessorType>

ProcessorType:
  name() -> string_view
  performance() -> double

Task:
  id() -> std::size_t
  period() -> Duration
  relative_deadline() -> Duration              // Can differ from period
  wcet() -> Duration                           // WCET in reference units
  wcet(ProcessorType) -> Duration              // Derived: wcet / speed_type

Job (value type, movable, created by Library 1, owned by Library 2):
  task() -> Task&                              // Back-reference to owning task
  remaining_work() -> Duration                 // In reference units
  total_work() -> Duration                     // Initial work (reference units)
  absolute_deadline() -> TimePoint             // arrival + relative_deadline
  consume_work(Duration amount) -> void        // Called by Library 1 (Processor)

Processor:
  id() -> std::size_t
  type() -> ProcessorType&
  state() -> State                             // Idle, Running, Sleep, Changing, ContextSwitching
  speed() -> double                            // Normalized: (f/f_max)*(p/p_ref)
  assign(Job&)                                 // throws if Sleep/Changing/busy
  clear()                                      // throws if Idle
  current_job() -> Job*                        // Job running on this processor
  current_task() -> Task*                      // Shortcut: current_job()->task()
  request_cstate(level)                        // Request a C-state level
  clock_domain() -> ClockDomain&
  power_domain() -> PowerDomain&
  set_isr_handler(IsrVector, handler)          // Per-processor ISR registration
  energy() -> Energy                           // Cumulative (if tracking on)

ClockDomain:
  id() -> std::size_t
  processors() -> span<Processor*>
  frequency() -> Frequency
  set_frequency(Frequency)                     // throws if out of range or locked
  lock_frequency()                             // Disable DVFS on this domain
  is_locked() -> bool
  freq_max() -> Frequency
  freq_min() -> Frequency
  transition_delay() -> Duration               // Configurable DVFS delay

PowerDomain:
  id() -> std::size_t
  processors() -> span<Processor*>
  c_states() -> span<CStateLevel>
```

---

## Library 2: Algorithms (`libschedsim-algo`)

### Responsibility

Library 2 provides scheduling and allocation algorithms built on top of
Library 1's primitives. It contains the "intelligence" of the simulation:
which task to run, when to change frequency, when to power down cores.

Library 2 depends on Library 1. Library 1 never depends on Library 2.

Library 2 is a single statically linked library.

### Scheduler-per-Processor Model

Every processor has exactly one scheduler. Multiple processors can share the
same scheduler instance:

```
Partitioned:   P0 → SchedA,  P1 → SchedB,  P2 → SchedC,  P3 → SchedD
Global:        P0 → SchedX,  P1 → SchedX,  P2 → SchedX,  P3 → SchedX
Mixed:         P0 → SchedX,  P1 → SchedX,  P2 → SchedY,  P3 → SchedZ
```

A scheduler's processor set is explicitly configured at construction.

```cpp
// Global scheduler for 4 processors
auto sched = EdfScheduler{engine, {p0, p1, p2, p3}};

// Partitioned: one scheduler per processor
auto sched0 = EdfScheduler{engine, {p0}};
auto sched1 = EdfScheduler{engine, {p1}};

// Mixed
auto global = EdfScheduler{engine, {p0, p1}};
auto part2  = EdfScheduler{engine, {p2}};
auto part3  = EdfScheduler{engine, {p3}};
```

### The EDF Scheduler Object

```cpp
#include <schedsim/core.hpp>
#include <schedsim/algo.hpp>

int main() {
    // Library 1: set up the platform
    auto engine = core::Engine{};
    auto& plt = engine.platform();

    auto& little = plt.add_processor_type("LITTLE", 1.0);
    auto& big    = plt.add_processor_type("big", 2.5);

    auto& clk_little = plt.add_clock_domain({200, 1200});
    auto& clk_big    = plt.add_clock_domain({400, 2000});
    auto& pwr = plt.add_power_domain({
        {.level=1, .scope=PerProcessor, .latency=1,   .power=50},
        {.level=3, .scope=DomainWide,   .latency=100, .power=5},
    });

    auto& p0 = plt.add_processor(little, clk_little, pwr);
    auto& p1 = plt.add_processor(little, clk_little, pwr);
    auto& p2 = plt.add_processor(big, clk_big, pwr);
    auto& p3 = plt.add_processor(big, clk_big, pwr);

    auto& t1 = engine.add_task({
        .period=10, .relative_deadline=10,
        .wcet=2.0               // reference units (wall time on big)
    });
    auto& t2 = engine.add_task({
        .period=20, .relative_deadline=15,    // constrained deadline
        .wcet=3.2
    });

    // Library 2: set up scheduling
    auto sched = algo::EdfScheduler{engine, {p0, p1, p2, p3}};
    sched.add_cbs_server(t1, {.budget=2.0, .period=10.0});
    sched.add_cbs_server(t2, {.budget=3.2, .period=20.0});
    sched.enable_grub();
    sched.set_dvfs_policy<algo::PowerAwarePolicy>();

    // Allocator routes all job arrivals to this scheduler
    auto alloc = algo::SingleSchedulerAllocator{engine, sched};

    // Run
    engine.run(100.0);
}
```

The `EdfScheduler` internally:
- Installs ISR handlers on its processors (job completion, deadline miss,
  processor available)
- Registers its bottom-half (deferred reschedule) callback
- Creates and manages CBS server state
- Implements EDF dispatch in its bottom-half handler
- Manages budget exhaustion timers via `engine.add_timer()`

### Task-to-Server Lookup

Library 2 maintains an internal map `Task* → CbsServer*`. When a Job
Completion ISR fires with a Task reference, Library 2 looks up the
corresponding server. When a Job Arrival comes through the allocator,
Library 2 finds or creates the server for that task.

### Allocators

Allocators are a Library 2 concept. **All job arrivals go through the
allocator**, including recurring arrivals for already-assigned tasks. This
gives the researcher the ability to do per-job reallocation.

```cpp
// Simple: all tasks go to one scheduler
auto alloc = algo::SingleSchedulerAllocator{engine, sched};

// Multi-scheduler: worst-fit allocation
auto alloc = algo::WorstFitAllocator{engine, {&sched1, &sched2}};

// Custom: researcher writes allocation logic
auto alloc = algo::CustomAllocator{engine, {&sched1, &sched2},
    [](Task& task, auto& schedulers) -> Scheduler& {
        // custom logic
        return *schedulers[0];
    }
};
```

The allocator installs the global Job Arrival ISR handler.

### DVFS and DPM Management

DVFS and DPM policies are managed by the scheduler. Each scheduler decides
when and how to change frequency or request C-states for its processors.

```cpp
// Inside scheduler: change frequency
proc.clock_domain().set_frequency(new_freq);

// Inside scheduler: request C-state for idle processor
proc.request_cstate(1);
```

Since multiple schedulers may share a clock domain, coordination is needed.
In practice, a single DVFS-aware scheduler manages all processors in a clock
domain, or a separate per-domain DVFS policy object observes multiple
schedulers.

### Task Model vs Server Model (Decoupled)

The task model and the server model are explicitly decoupled:

**Task (Library 1):**
- WCET in reference units (wall time on reference processor at max freq)
- Period (T_task)
- Relative deadline (D_task, can differ from period)
- Jobs: each job has its own actual execution time (<= WCET)
- WCET = max execution time across all jobs of this task

**CBS Server (Library 2):**
- Budget (Q_s): maximum execution time per server period
- Period (T_s): server replenishment period (can differ from task period)
- Utilization: U_s = Q_s / T_s (derived, not a primary parameter)
- Virtual time, deadline, state machine

The server is parameterized by **(budget, period)**, not utilization.
Utilization is derived. This matches CBS theory and allows:
- Server period different from task period
- Multiple tasks sharing a server (future)
- Explicit budget control for research

```cpp
// Library 2: CBS server with explicit budget and period
sched.add_cbs_server(task, {.budget=2.0, .period=10.0});

// Convenience: derive from task parameters
sched.add_cbs_server(task);  // budget=WCET(ref), period=task.period()
```

The admission test checks: total server utilization <= capacity bound.
The schedulability requirement: `Q_s / T_s >= WCET / T_task`.

Library 1's Task does NOT reference Server. The association is maintained
entirely within Library 2 (via internal map).

### Algorithm Modules

#### CBS (Constant Bandwidth Server)

Server lifecycle, budget tracking, virtual time.
- Server parameters: budget Q, period T
- Utilization: U = Q / T
- Virtual time: `vt + rt / U`
- Budget remaining: starts at Q, decremented as task runs
- Replenishment: when deadline reached, budget replenished to Q

**CBS server states: {Inactive, Ready, Running}**
```
              job arrival              dispatched (EDF)
  Inactive ──────────────► Ready ─────────────────────► Running
      ▲                      ▲                           │   │
      │                      │ budget exhausted          │   │
      │                      │ (postpone deadline)       │   │
      │                      └───────────────────────────┘   │
      │                                                      │
      │     job completed (no more jobs)                     │
      └──────────────────────────────────────────────────────┘
```
On job completion: if there is another job in the queue, the server
stays Ready (Library 2 manages the queue). Otherwise it goes Inactive.

#### GRUB (Greedy Reclamation of Unused Bandwidth)

Extends CBS. Overrides formulas with bandwidth reclaiming:
- Virtual time: `vt + (bw / U) * rt`
- Budget: `(U / bw) * (d - vt)`
- Where `bw` = current bandwidth factor

**GRUB adds a fourth state: NonContending.** When a job completes early
(virtual time < deadline), the server enters NonContending instead of
Inactive. The server's bandwidth is redistributed to other active
servers. When the deadline is reached, the server transitions to
Inactive and bandwidth is reclaimed.

```
GRUB states: {Inactive, Ready, Running, NonContending}

  Running ──► job completed early ──► NonContending
                                           │
                                           │ deadline reached
                                           ▼
                                        Inactive
```

**Implementation**: The NonContending state exists in the server state
enum but is only used when GRUB is enabled. The EdfScheduler holds an
optional GRUB module. When enabled, GRUB overrides the job completion
handler to transition to NonContending instead of Inactive (when the
server completed early). Pure CBS code never enters NonContending.

#### EDF (Earliest Deadline First)

Dispatch policy. Sorts ready servers by deadline, assigns to processors.

#### DVFS Policies

- **PowerAware**: frequency proportional to active utilization
- **CSF**: combined with DPM, frequency/core scaling
- **FFA**: combined with DPM, fixed frequency steps
- **Timer variants**: deferred frequency changes with cooldown timers

#### DPM Policies

Manage processor C-states based on scheduler load.

### Dependency Graph

```
CBS ←── GRUB (extends CBS formulas)
CBS ←── EDF (uses server state for dispatch, does not modify CBS)
EDF ←── DVFS policies (use utilization info from CBS/GRUB)
EDF ←── DPM policies (use load info, manage processors)
```

### How Library 2 Uses Library 1

1. **ISR handlers**: Per-processor for Job Completion, Deadline Miss,
   Processor Available. Global for Job Arrival (via allocator).
2. **Processor API**: `assign()`, `clear()`, `request_cstate()`
3. **Clock Domain API**: `set_frequency()`
4. **Deferred API**: `register_deferred()`, `request_deferred()`
5. **Timer API**: `add_timer()`, `cancel_timer()`
6. **Query API**: `speed()`, `remaining_work()`, `state()`, `frequency()`,
   etc.

---

## Library 3: I/O (`libschedsim-io`)

### Responsibility

Library 3 handles configuration loading and result output. It is separate
from Libraries 1 and 2 so that the core simulation and algorithms have no
I/O dependencies.

Library 3 depends on Library 1 and Library 2.

### Contents

- **Platform loader**: Load platform configuration from JSON
  (processor types, clock domains, power domains, processors)
- **Scenario loader**: Load task sets and job arrival schedules from JSON
- **Trace writers**: Concrete trace sink implementations
  - JSON file writer
  - In-memory buffer (for post-processing without disk I/O)
  - Socket/DB writer (future)
- **Result exporters**: Convert simulation results to various formats

### Output Performance

Writing to disk is often the bottleneck. Library 3 provides options:
- **No output**: null trace sink, maximum performance
- **In-memory**: traces stored in a buffer, processed after simulation ends.
  Avoids disk I/O during simulation.
- **Streaming JSON**: write traces to file during simulation
- **Socket/DB**: send traces over network to a database (future work)

The researcher chooses the output strategy based on their needs. For large
parameter sweeps, in-memory processing without disk I/O is critical.

---

## Mapping: Current Code to New Architecture

### Current event → New ISR model

| Current Event Variant | New Model |
|---|---|
| `events::JobArrival` | Core ISR: Job Arrival (routed through allocator) |
| `events::JobFinished` | Core ISR: Job Completion |
| `events::ServBudgetExhausted` | Library 2 timer closure (CBS budget) |
| `events::ServInactive` | Library 2 internal CBS state transition |
| `events::TimerIsr` | Library 2 timer closure |

### Current class → New location

| Current Class | Library 1 | Library 2 | Library 3 |
|---|---|---|---|
| `Engine` | Engine | — | — |
| `Task` | Task | — | — |
| `Processor` | Processor | — | — |
| `Cluster` | Removed → ClockDomain + PowerDomain | — | — |
| `Platform` | Platform | — | — |
| `Server` | — | CBS Server | — |
| `Scheduler` (base) | — | EdfScheduler | — |
| `Parallel` | — | EDF + GRUB | — |
| `PowerAware` | — | PowerAware DVFS | — |
| `DpmDvfs` | — | DPM module | — |
| Scenario/hardware JSON | — | — | Loaders |
| Trace output | Trace sink API | — | Writers |

### Current method → New location

| Current Method | New Location | Reason |
|---|---|---|
| `update_server_times` | Library 2 (CBS) | Virtual time is CBS |
| `activate_alarms` | Library 2 (CBS) | Budget timers are CBS |
| `cancel_alarms` | Library 2 (CBS) | Budget timers are CBS |
| `resched_proc` | Library 2 (EDF) | Dispatch decision |
| `deadline_order` | Library 2 (EDF) | EDF comparison |
| `on_job_arrival` | Library 2 (CBS via allocator) | Server creation is CBS |
| `on_job_finished` | Library 2 (CBS) handles ISR | Server state transition |
| `detach_server_if_needed` | Library 2 (CBS) | Server lifecycle |
| `admission_test` | Library 2 (CBS/GRUB) | Algorithm-specific |
| `update_platform` | Library 2 (DVFS policy) | Frequency policy |
| `adjust_active_processors` | Library 2 (DPM policy) | Core management |

---

## Design Decisions

1. **Three libraries, statically linked.** Core (mechanism), Algorithms
   (policy), I/O (configuration and output).

2. **ISR model for extensibility.** Library 1 fires ISRs for core events.
   Library 2 installs handlers.

3. **Core ISR vectors: Job Arrival, Job Completion, Deadline Miss, Processor
   Available.** CBS-specific events become Library 2 timer closures.

4. **Deadline Miss ISR.** Library 1 detects deadline misses and fires an ISR,
   allowing Library 2 to react at runtime.

5. **Timer closures.** Timers use `std::function<void()>`. The closure
   captures context (server pointer, etc.). No separate timer dispatch table.

6. **Timers and ISRs share one priority queue.** Within a time step, events
   are ordered by configurable priority. Default: Job Completion > Deadline
   Miss > Processor Available > Timer callbacks > Job Arrival.

7. **No same-time timer events.** Timers must be scheduled strictly in the
   future.

8. **ISR routing is per-processor.** Except Job Arrival, which is global
   (routed to the allocator).

9. **All job arrivals go through the allocator.** Including recurring
   arrivals for already-assigned tasks. This enables per-job reallocation.

10. **Deferred callbacks ("last event" callbacks).** Library 1 provides
    `register_deferred()` / `request_deferred()`. Callbacks fire after all
    events at the current time step. Deduplicated (fires once per time step).
    Order is deterministic: by registration order.

11. **Library 1 handles execution tracking.** Job completion detection is
    core. Library 1 decrements remaining time, handles preemption, adjusts
    for frequency changes, and fires the completion ISR.

12. **Time management is Library 1 only.** Future switch from double to
    discrete integer time will not affect Library 2.

13. **Jobs are a core data concept.** Job objects (execution time,
    remaining work, deadline) live in Library 1. The job queue
    (ordering, advancement, overrun policy) is managed by Library 2.
    See decision 59.

14. **CBS Servers are Library 2.** Library 1's Task does not reference Server.
    Library 2 maintains a `Task* → CbsServer*` map.

15. **EdfScheduler is the main Library 2 object.** Researchers create
    instances, add CBS servers, enable GRUB, set DVFS/DPM policies.

16. **Scheduler-per-processor model.** Every processor has one scheduler.
    Supports global, partitioned, and mixed scheduling.

17. **No Cluster concept.** Replaced by Clock Domains and Power Domains.

18. **C-state levels with domain constraints.** Per-processor or domain-wide.

19. **Processor Available ISR for DVFS, DPM, and context switch.** Fired
    when a processor becomes available after any transition.

20. **Default platform state is maximum performance.** Max frequency, all
    processors active.

21. **Allocators are Library 2.** Route job arrivals to schedulers.

22. **DVFS and DPM are scheduler-managed.** Coordination needed for shared
    clock domains.

23. **Single WCET in reference units; per-type wall times derived.** Tasks
    specify a single WCET in reference units (wall time on reference processor
    at max frequency). Per-type wall times are derived: `wall_type = wcet / speed_type`.

24. **Processor types are Library 1.** Name + performance factor.

25. **Context switch overhead.** Optional, configurable per processor type.
    Modeled as a brief delay in Library 1.

26. **Energy tracking.** Optional, integrated in Library 1. Tracks power
    consumption based on frequency, C-state, and activity. Disabled by
    default for performance.

27. **Simulation termination.** Three modes: fixed duration, empty queue,
    condition callback.

28. **Deterministic simulation.** All ordering is deterministic. Bottom-half
    order follows processor declaration order. ISR priority is configurable
    but fixed at setup time.

29. **Trace sink.** Library 1 provides a generic type-erased trace sink API.
    Library 3 provides concrete implementations (JSON, in-memory, socket).
    Library 2 emits algorithm traces through the same sink.

30. **Library 3 handles I/O.** JSON loading, result writing, network output.
    Keeps Libraries 1 and 2 free of I/O dependencies.

31. **Task and server models are decoupled.** Task has WCET, period,
    relative_deadline. CBS server has budget, period. Server utilization
    is derived (Q/T). Server period can differ from task period.

32. **CBS server parameterized by (budget, period).** Not by utilization.
    Matches CBS theory. Utilization = budget / period is derived.

33. **Task relative deadline.** Tasks specify a relative deadline that can
    differ from period (constrained deadlines). Absolute deadline =
    arrival_time + relative_deadline.

34. **Job actual execution time.** Each job has its own execution time
    (from scenario), which can be less than WCET. WCET is the maximum
    across all jobs. Library 1 tracks the job's actual execution time.

35. **Writer-callback trace system.** Library 1 defines a TraceWriter
    interface (field-by-field). Traces are emitted via lambdas. No heap
    allocation per trace. Library 3 provides concrete writers.

36. **Frequency and C-state transition delays.** Configurable per clock
    domain (DVFS transition delay) and per C-state level (wake-up latency).
    Part of the platform description.

37. **Jobs are explicit in the scenario.** Every job arrival is listed in the
    scenario file with (task, arrival_time, execution_time). Library 1 does
    not auto-generate periodic arrivals.

38. **Fail-fast error handling.** Library 1 returns errors (or throws) on
    invalid API calls rather than silently doing nothing:
    - `proc.assign()` on a sleeping/changing processor → error
    - `proc.assign()` on a processor that already has a task → error
    - `clock_domain.set_frequency()` out of range → error
    - `engine.cancel_timer()` on already-fired timer → no-op (not an error)

39. **Power model (initial).** ~~Active and idle power consumption are equal
    (no distinction for now). Sleep power consumption is 0.~~ Superseded by
    decision 86: polynomial model for active/idle power, configurable
    C-state power levels.

40. **Namespaces.** `schedsim::core` (Library 1), `schedsim::algo`
    (Library 2), `schedsim::io` (Library 3).

41. **Migration strategy.** Write Library 1 from scratch with full tests and
    documentation. Then build Library 2 on top with a basic EDF scheduler
    to validate the API. Verify that combined output matches the current
    simulator's traces.

42. **Library 1 is fully tested and documented.** Unit tests for every
    component. API documentation for every public method.

43. **C++20 standard.** The codebase targets C++20. This gives us concepts,
    ranges, `std::span`, `constexpr` improvements. C++23 features like
    `std::expected` are not used for portability across compilers.

44. **Error handling: exceptions + debug assertions.** Public API misuse
    (assign to sleeping processor, out-of-range frequency) throws exceptions.
    Internal invariants use `assert()` — active in debug builds, compiled out
    in release. No error codes; callers handle exceptions or let them
    propagate as crashes (fail-fast).

45. **Time via `std::chrono` with custom `SimClock`, strong types for
    physical quantities.** A custom clock type provides compile-time
    separation between `TimePoint` (absolute) and `Duration` (relative):
    ```cpp
    namespace schedsim::core {
        struct SimClock {
            using rep        = double;
            using period     = std::ratio<1>;   // 1 second
            using duration   = std::chrono::duration<rep, period>;
            using time_point = std::chrono::time_point<SimClock>;
            static constexpr bool is_steady = true;
        };

        using Duration  = SimClock::duration;    // relative intervals
        using TimePoint = SimClock::time_point;  // absolute from epoch 0

        struct Frequency { double mhz; /* operators */ };
        struct Power     { double mw;  /* operators */ };
        struct Energy    { double mj;  /* operators */ };
    }
    ```
    This gives: `TimePoint - TimePoint = Duration`,
    `TimePoint + Duration = TimePoint`, `TimePoint + TimePoint` = compile
    error. Future switch to discrete time: change `rep` to `int64_t` and
    `period` to e.g. `std::nano`.

46. **Event queue: `std::map` with composite key.** The event queue is a
    `std::map<EventKey, Event>` where `EventKey` orders by
    (time, priority, sequence). The sequence number makes every key unique,
    so `std::map` suffices (no multimap). Supports O(log n) insertion and
    O(1) deletion by iterator. Timer cancellation erases the entry
    directly. To be benchmarked later; may switch to a different structure
    if profiling shows it as a bottleneck.
    ```cpp
    struct EventKey {
        TimePoint time;       // Primary: simulation time
        int       priority;   // Secondary: lower = fires first
        uint64_t  sequence;   // Tertiary: insertion order (determinism)
        auto operator<=>(const EventKey&) const = default;
    };
    std::map<EventKey, Event> event_queue_;
    ```

47. **Arena-like lifetime, `std::vector` with `finalize()`.** All
    simulation objects (processors, tasks, clock domains, etc.) are created
    during setup and destroyed together when the Engine is destroyed. No
    dynamic allocation during simulation. Objects are stored in
    `std::vector` (contiguous, cache-friendly). The Platform/Engine exposes
    a `finalize()` method that locks the collections — no more additions
    allowed after finalization. Pointers/references taken after `finalize()`
    are stable for the simulation lifetime. `finalize()` is called
    automatically by `engine.run()` if not called explicitly.

48. **GoogleTest for unit tests.** Library 1 and Library 2 use GoogleTest
    for native C++ unit tests. Integration/regression tests (trace
    checksums) remain in Python (`verify_grub.py`).

49. **Custom `SimClock` for type-safe time.** `TimePoint` and `Duration`
    are distinct types via `std::chrono::time_point<SimClock>` vs
    `SimClock::duration`. The compiler rejects `TimePoint + TimePoint`
    and requires explicit conversions. CBS formulas work naturally:
    `Duration / Duration = double`, `double * Duration = Duration`,
    `TimePoint + Duration = TimePoint`.

50. **`std::vector<T>` by value, performance first.** Processors, tasks,
    clock domains, power domains are stored by value in `std::vector<T>`
    inside Platform/Engine. Contiguous memory layout, no pointer
    indirection. Elements are non-copyable but movable (move needed for
    vector reallocation during setup). After `finalize()`, no insertions
    or moves occur — all addresses are stable for the simulation lifetime.

51. **Event representation: `std::variant`.** Events stored in the queue
    use `std::variant`, same approach as the current codebase (fast
    dispatch via `std::visit`, no heap allocation per event). Core ISR
    events carry their specific data. Timer events carry a closure.
    ```cpp
    struct JobArrivalEvent       { Task* task; Duration exec_time; };
    struct JobCompletionEvent    { Processor* proc; Job* job; };
    struct DeadlineMissEvent     { Processor* proc; Job* job; };
    struct ProcessorAvailableEvent { Processor* proc; };
    struct TimerEvent            { std::function<void()> callback; };

    using Event = std::variant<
        JobArrivalEvent, JobCompletionEvent, DeadlineMissEvent,
        ProcessorAvailableEvent, TimerEvent>;
    ```
    The engine dispatches via `std::visit`: core ISR events call the
    corresponding handler installed on the relevant processor; timer events
    invoke the closure directly.

52. **`TimerId` wraps a map iterator.** Timer cancellation erases the
    iterator directly — O(1). A validity flag prevents double-cancel.
    Iterators into `std::map` remain valid after other insertions/erasures.
    If the event queue implementation changes later, `TimerId` becomes an
    opaque handle backed by a different mechanism.

53. **`proc.clear()` on Idle throws.** Calling `clear()` on a processor
    that has no task is a programming error and throws an exception. This
    may be relaxed to a no-op later if profiling shows the branch is
    costly in hot paths.

54. **Composite event key (experimental).** The three-level ordering
    (time → priority → sequence) is the initial approach. To be validated
    during Phase 1 implementation. If the composite key causes issues
    (performance, complexity), it can be simplified.

55. **Single-pass deferred callbacks.** After all events at time T are
    dispatched, deferred callbacks fire in a single pass (registration
    order). If a deferred callback calls `request_deferred()` for another
    callback, that second callback does NOT fire in the same pass — it
    fires after the next batch of events (at the same or a later time
    step). This prevents infinite loops and makes behavior predictable.
    **Document this clearly in the API.**

56. **`schedule_job_arrival()` is a first-class Engine method.** Job
    arrivals are injected via `engine.schedule_job_arrival(task,
    arrival_time, exec_time)`. This creates a `JobArrivalEvent` in the
    event queue at `arrival_time`. The engine fires the Job Arrival ISR
    (routed to the allocator). User code or Library 3 loaders call this
    during setup for every job in the scenario.

57. **Event variant for ISR dispatch.** Core ISR events use `std::variant`
    (no heap allocation, fast `std::visit` dispatch). Timer events carry a
    `std::function<void()>` closure. The engine dispatches core ISR events
    to per-processor handlers; timer events invoke the closure directly.
    Per-processor handlers are stored as typed `std::function` fields:
    ```cpp
    class Processor {
        std::function<void(Processor&, Job&)>        on_job_completion_;
        std::function<void(Processor&, Job&)>        on_deadline_miss_;
        std::function<void(Processor&)>              on_processor_available_;
    };
    ```

58. **Library 1 computes normalized processor speed.** Each processor's
    execution speed is normalized so that the reference processor at
    maximum frequency has speed = 1.0:
    `speed = (freq / freq_max) * (perf / perf_ref)`.
    This allows remaining execution time to be stored directly in
    reference units (wall time on reference type at max frequency) with
    no conversion needed. Library 2 never computes speed directly.

59. **Job queue is managed by Library 2.** Library 1 creates Job objects
    (they are core data) and fires the Job Arrival ISR. But the queue
    (ordering, advancement, overrun policy) is Library 2's responsibility.
    On job completion, Library 1 fires the Job Completion ISR but does
    NOT auto-advance the queue. Library 2 explicitly decides which job
    runs next. This gives Library 2 full control over server-job
    associations, overrun policies, and multi-job scheduling. Updates
    decision 13: jobs remain a core concept (data), but queue management
    moves to Library 2.

60. **One allocator per engine (runtime enforcement).** The Engine stores
    a single Job Arrival handler. The allocator installs itself as this
    handler during construction. If a second allocator tries to install,
    the Engine throws. Compile-time enforcement is not practical since
    the allocator is a runtime object.

61. **Elements are non-copyable, movable, stored by value.** Processor,
    Task, ClockDomain, PowerDomain are non-copyable (deleted copy
    constructor/assignment) but movable (required by `std::vector`
    reallocation during setup). After `finalize()`, the vectors are
    locked and no moves occur — pointers/references are stable for the
    simulation lifetime. By-value storage in contiguous `std::vector`
    means no heap indirection, optimal cache performance during
    simulation.

62. **Execution tracking in reference units.** Library 1 tracks remaining
    execution in **reference units** (wall time on reference processor
    type at maximum frequency). Speed is normalized:
    `speed = (freq / freq_max) * (perf / perf_ref)`, so that reference
    at max freq has speed = 1.0. This handles migration naturally:
    - `consumed = wall_elapsed * speed`
    - `completion_time = now + remaining / speed`
    - On migration: `remaining` unchanged, `speed` changes
    - On DVFS: `speed` changes, Library 1 recomputes completion
    - Scenario specifies job execution time in reference units
    - Per-type wall times derived: `wcet_type = wcet / speed_type`
    ```
    Example:
      big (perf=2.5, reference), LITTLE (perf=1.0)
      Job: remaining = 2.0 ref units
      big at max:    speed = 1.0*1.0 = 1.0,  wall = 2.0
      LITTLE at max: speed = 1.0*0.4 = 0.4,  wall = 5.0

      Migration scenario:
      t=0:   assign to big (speed=1.0), completion at t=2.0
      t=0.8: preempt. consumed=0.8*1.0=0.8. remaining=1.2
      t=0.8: assign to LITTLE (speed=0.4), completion at t=0.8+3.0=3.8
      t=3.8: complete. consumed=3.0*0.4=1.2. total work=2.0 ✓
    ```

63. **`proc.speed()` exposed to Library 2.** Library 2 queries normalized
    processor speed via `proc.speed()` (read-only). Library 2 uses this
    to compute CBS budget exhaustion timers:
    `exhaust_time = now + remaining_budget / speed`. Library 2 does not
    implement the speed formula.

64. **CBS server states: {Inactive, Ready, Running}. GRUB adds
    NonContending.** Pure CBS has three states. The NonContending state
    (job completed early, bandwidth redistributed) is a GRUB extension.
    The server state enum includes NonContending but CBS code never
    transitions to it. When GRUB is enabled on the scheduler, the job
    completion handler transitions to NonContending instead of Inactive
    when `virtual_time < deadline`. This keeps CBS minimal and GRUB
    opt-in.

65. **Lifecycle flow requires integration testing.** The job lifecycle
    (arrival → dispatch → completion → budget exhaustion → migration)
    must be validated with integration tests comparing against known
    correct results after implementation.

66. **Three separate `run()` methods.** `run()` (until empty),
    `run(TimePoint until)` (until time), `run(std::function<bool()>
    stop)` (until condition). Separate overloads, not a single method
    with variant or optional parameters.

67. **Two separate timer APIs: public and internal.** Library 2 uses
    `engine.add_timer(TimePoint, int priority, closure) -> TimerId`.
    Library 1 uses `engine.add_internal_event(...)` (private or friend)
    for core ISR events. Library 1's internal priorities are in a
    reserved range (e.g., negative values or a dedicated low range)
    that Library 2 cannot use. This prevents Library 2 from accidentally
    preempting core ISR dispatch. If a researcher needs more control,
    they modify Library 1 directly for their purposes.
    Default internal priorities: Job Completion < Deadline Miss <
    Processor Available < Job Arrival.

68. **Engine is stack-allocated, non-movable, non-copyable.** Engine
    holds Platform by value, and Platform/Processors hold `Engine&`
    back-references. Moving Engine would invalidate these references.
    Engine deletes copy and move constructors. Users create it on the
    stack: `auto engine = schedsim::core::Engine{};`

69. **DVFS transition pauses execution.** When `set_frequency()` is
    called on a clock domain, processors enter Changing state. If a
    processor was Running, execution is paused (consumed work updated,
    completion event cancelled). After the transition delay, the
    processor resumes at the new speed (completion rescheduled) and the
    Processor Available ISR fires.

70. **Simple ID types.** Task, Processor, ClockDomain, PowerDomain IDs
    are plain `std::size_t`. No strong-type wrapper to avoid
    over-engineering. IDs are assigned sequentially during `add_*()`
    calls, starting from 0.

71. **New code in same repository.** The new `schedsim/` directory lives
    alongside the existing `schedlib/` directory. The old code stays
    untouched during development. Directory hierarchy can be reorganized
    later.

72. **`proc.assign(Job&)` not `proc.assign(Task&)`.** Since Library 2
    manages the job queue (decision 59), Library 2 picks the specific
    Job to run and passes it to Library 1. Job has a back-reference to
    its Task, so Library 1 can still access the Task when needed.

73. **Job ownership: created by Library 1, owned by Library 2.** Library
    1 creates Job objects when `JobArrivalEvent` fires and moves them to
    Library 2 via the ISR handler `(Task&, Job)`. Library 2 stores them
    in its CBS server queue. When `proc.assign(job)` is called, Library
    1 stores a `Job*` and updates remaining work via
    `job.consume_work(amount)`. No friend relationship needed — Job
    exposes a public `consume_work(Duration)` method. Library 1 calls
    it at specific points: `clear()`, DVFS change, completion.

74. **Exception naming convention: `<Description>Error`.** All exception
    types follow the pattern `<Description>Error` and inherit from a
    base `SimulationError` (which inherits `std::runtime_error`).
    Consistent across all three libraries:
    - Library 1: `InvalidStateError`, `OutOfRangeError`,
      `AlreadyFinalizedError`, `HandlerAlreadySetError`
    - Library 2: `AdmissionError`, etc.

75. **Build: Nix-optional, CMake-first.** CMakeLists.txt uses
    `find_package(GTest)` first (works with Nix-provided or
    system-installed GoogleTest). Falls back to `FetchContent` if not
    found (for non-Nix users). The project builds with CMake alone — Nix
    is a convenience, not a requirement.
    - Nix users: `flake.nix` provides GTest, CMake, compiler in devShell
    - Non-Nix users: FetchContent downloads GoogleTest automatically
    - CI: can use Nix-based or standard Ubuntu (FetchContent handles deps)

76. **Multiple schedulers on same clock domain: allowed, DVFS locked.**
    Multiple schedulers can manage different processors in the same clock
    domain. In that case, `set_frequency()` is disabled (throws). Library
    1 provides `clock_domain.lock_frequency()` — a setup-time call that
    permanently disables frequency changes on that domain. Library 2 or
    the researcher calls it when multiple schedulers share a domain.
    DVFS is only available when a single scheduler owns all processors
    in a clock domain.

77. **Heterogeneous clock domains in one scheduler: not supported.**
    A scheduler's processor set must have uniform speed (same clock
    domain, or multiple domains at the same frequency). If a researcher
    creates a scheduler spanning clock domains at different frequencies,
    GRUB/CBS throws an error. This constraint may be relaxed in future
    research.

78. **C API layer for multi-language wrappers.** A thin `extern "C"` API
    layer wraps the C++ libraries. Language bindings derive from this
    single source:
    - Python: `ctypes` (built-in, no dependency) or `cffi`
    - Rust: `bindgen` (auto-generates Rust FFI from C headers)
    No need to maintain separate wrappers per language. The C API is the
    stable ABI. Implementation deferred to after Phase 7.

79. **Extensible public interfaces: Scheduler and Allocator.** Library 2
    defines abstract base classes that researchers implement without
    modifying any library source:
    ```cpp
    // Library 2 defines:
    class Scheduler {
    public:
        virtual void on_job_arrival(Task& task, Job job) = 0;
        virtual bool can_admit(Duration budget, Duration period) const = 0;
        virtual double utilization() const = 0;
        virtual ~Scheduler() = default;
    };

    class Allocator {
    public:
        virtual void on_job_arrival(Task& task, Job job) = 0;
        virtual ~Allocator() = default;
    };
    ```
    Researchers subclass these in their own code:
    ```cpp
    class MyScheduler : public algo::Scheduler { ... };
    class MyAllocator : public algo::Allocator { ... };
    ```
    Library 2's `EdfScheduler`, `SingleSchedulerAllocator`, etc. are
    concrete implementations of these interfaces.

80. **Traces: auto-timestamp, zero-overhead when disabled.** Library 1
    automatically adds the current simulation time to every trace. The
    caller only provides type and fields. When no trace writer is set,
    `engine.trace()` is a single branch (check null pointer) — zero
    overhead. The trace lambda is never evaluated if no writer is set.
    ```cpp
    template<typename F>
    void Engine::trace(F&& fn) {
        if (trace_writer_) {
            trace_writer_->begin(current_time_);
            fn(*trace_writer_);
            trace_writer_->end();
        }
    }
    ```

81. **Energy tracking: update on every transition, no lazy updates.**
    Energy is accumulated on every state change event:
    `energy += power(state) * (now - last_change_time)`. This happens
    at every transition (assign, clear, DVFS change, C-state change),
    even if time delta is zero (no-op in that case). No lazy evaluation
    — energy is always up-to-date when queried. This prevents tracking
    errors from multiple transitions within the same timestep.

82. **ContextSwitching: fifth processor state.** When context switch
    overhead is enabled, `proc.assign(job)` puts the processor in
    `ContextSwitching` state for the configured delay. After the delay,
    the processor transitions to Running and execution begins. The
    Processor Available ISR fires when switching completes. The state
    name explicitly communicates "context switch" to the user.
    States: {Idle, Running, Sleep, Changing, ContextSwitching}.

83. **Libraries are extensible without modification.** All scheduling
    mechanics (dispatch, formulas, policies) are overridable by the
    researcher through subclassing or composition. Concretely:
    - Custom scheduler: subclass `algo::Scheduler`
    - Custom allocator: subclass `algo::Allocator`
    - Custom GRUB formulas: subclass `EdfScheduler`, override virtual
      methods for `compute_virtual_time()`, `compute_budget()`
    - Custom DVFS/DPM policy: implement a policy interface
    The researcher's code links against the libraries and extends them.
    No library source modification needed.

84. **DVFS policy: delegation pattern with abstract interface.** DVFS
    frequency scaling is handled by a `DvfsPolicy` abstract interface.
    EdfScheduler holds an optional `unique_ptr<DvfsPolicy>`. Built-in
    policies (PowerAware, CSF, FFA, and their timer variants) implement
    this interface. Researchers subclass `DvfsPolicy` for custom
    frequency scaling without modifying EdfScheduler.
    ```cpp
    class DvfsPolicy {
    public:
        // Called when active utilization changes (after deferred callbacks)
        virtual void on_utilization_changed(Scheduler& sched, ClockDomain& domain) = 0;
        // Called when a processor becomes idle
        virtual void on_processor_idle(Scheduler& sched, Processor& proc) = 0;
        // Optional: cooldown for timer-based policies
        virtual Duration cooldown_period() const { return Duration{0}; }
        virtual ~DvfsPolicy() = default;
    };
    ```
    Default: `NoDvfsPolicy` (no-op, platform stays at configured frequency).

85. **DPM policy: same delegation pattern.** `DpmPolicy` is an abstract
    interface for C-state management:
    ```cpp
    class DpmPolicy {
    public:
        virtual void on_processor_idle(Scheduler& sched, Processor& proc) = 0;
        virtual void on_processor_needed(Scheduler& sched) = 0;
        virtual ~DpmPolicy() = default;
    };
    ```
    DPM and DVFS policies are independent — a researcher can combine any
    DVFS policy with any DPM policy. Default: `NoDpmPolicy` (processors
    stay in C0, never enter deep sleep).

86. **Power model: polynomial + C-state power.** Active power uses the
    existing polynomial model `P(f) = a0 + a1*f + a2*f² + a3*f³` where
    coefficients are configured per clock domain. C-state power is
    configured per power domain. Total power:
    - Running: `P_poly(freq)` (polynomial at current frequency)
    - Idle (C0): `P_poly(freq)` (same as running for simplicity, or a
      configured idle fraction)
    - Sleep (C1+): `P_cstate(level)` (configured per C-state level)
    See decision 98 for the platform JSON schema with clock_domains,
    power_domains, and processors. If no C-states configured, processors
    cannot enter sleep (C0 only).

87. **Deferred callback ordering: registration order, documented.**
    Deferred callbacks fire in registration order, which follows
    scheduler construction order. Since each processor has exactly one
    scheduler (decision 16), ordering affects only global timing — not
    correctness. This is deterministic and predictable. No priority
    parameter is needed. **Document clearly in API**: "Deferred
    callbacks fire once per timestep, in the order they were registered.
    Registration order follows scheduler construction order."

88. **Timer-based DVFS is internal to policy.** Cooldown timers are
    implementation details of timer policy variants (PowerAwareTimerPolicy,
    CsfTimerPolicy, FfaTimerPolicy), not a separate class hierarchy. Each
    timer policy manages its own `TimerId` for pending frequency changes.
    Built-in policies:
    - `NoDvfsPolicy` — no-op (default)
    - `PowerAwarePolicy` — immediate frequency scaling
    - `PowerAwareTimerPolicy` — deferred with cooldown
    - `CsfPolicy`, `CsfTimerPolicy` — CSF algorithm
    - `FfaPolicy`, `FfaTimerPolicy` — FFA algorithm

89. **EdfScheduler composition: CBS core + policy extensions.** EdfScheduler
    owns CBS servers directly (by value in `std::vector`). Extensions are
    pluggable via policy interfaces:
    ```cpp
    class EdfScheduler : public Scheduler {
        std::vector<CbsServer> servers_;
        std::unordered_map<Task*, CbsServer*> task_to_server_;
        std::unique_ptr<ReclamationPolicy> reclaim_policy_;  // GRUB, CASH, IRIS
        std::unique_ptr<DvfsPolicy> dvfs_policy_;
        std::unique_ptr<DpmPolicy> dpm_policy_;
    public:
        void set_reclamation_policy(std::unique_ptr<ReclamationPolicy> p);
        void set_dvfs_policy(std::unique_ptr<DvfsPolicy> p);
        void set_dpm_policy(std::unique_ptr<DpmPolicy> p);
        // Convenience
        void enable_grub() { set_reclamation_policy(make_unique<GrubPolicy>()); }
        void enable_cash() { set_reclamation_policy(make_unique<CashPolicy>()); }
    };
    ```

90. **ReclamationPolicy interface for CBS extensions.** Bandwidth reclamation
    mechanisms (GRUB, CASH, IRIS) are implemented via a `ReclamationPolicy`
    interface. Pure CBS uses no reclamation policy (null). Extension points:
    ```cpp
    class ReclamationPolicy {
    public:
        // Called when job completes before budget exhausted
        // Returns: should server enter NonContending state?
        virtual bool on_early_completion(CbsServer& srv, Duration remaining) = 0;

        // Called when budget exhausted but job not done
        // Returns: extra budget granted (0 = standard CBS postpone)
        virtual Duration on_budget_exhausted(CbsServer& srv) = 0;

        // Virtual time computation (override for GRUB)
        virtual TimePoint compute_virtual_time(
            const CbsServer& srv, TimePoint vt, Duration exec) {
            return vt + exec / srv.utilization();  // Standard CBS
        }

        // State change notification (for tracking active bandwidth)
        virtual void on_server_state_change(
            CbsServer& srv, State old_state, State new_state) {}

        virtual ~ReclamationPolicy() = default;
    };
    ```
    This allows GRUB (modified VT formula), CASH (spare queue), and IRIS
    (fair distribution) to be implemented without modifying EdfScheduler.

91. **No real-time metrics tracking.** Library 2 does not store historical
    statistics (deadline miss counts, preemption counts, etc.). These are
    derived from traces by Library 3 post-processing. Exception: active
    utilization is tracked in real-time for GRUB-PA DVFS decisions.

92. **Scenario generation in Library 3.** Library 3 provides task set
    generation utilities:
    ```cpp
    namespace schedsim::io {
        // UUniFast algorithm
        std::vector<double> uunifast(size_t n, double U, std::mt19937& rng);

        // Generate task parameters
        std::vector<TaskParams> generate_task_set(
            size_t n_tasks, double target_U,
            Duration period_min, Duration period_max,
            std::mt19937& rng);

        // Generate job arrivals for duration
        std::vector<JobArrivalParams> generate_arrivals(
            const std::vector<TaskParams>& tasks,
            Duration duration, std::mt19937& rng,
            double exec_ratio = 1.0);  // actual/wcet
    }
    ```

93. **Migration = clear() + assign().** No special migration API. Library 2
    migrates by calling `from.clear()` then `to.assign(job)`. Library 1
    handles mechanics: `clear()` updates remaining work, `assign()` computes
    completion at new speed. Remaining work in reference units ensures
    correctness across processor types. Library 2 emits migration trace.

94. **Job queue per CbsServer.** Each server holds `std::deque<Job>` for
    queued jobs. Overrun policy is configurable per-server:
    ```cpp
    enum class OverrunPolicy { Queue, Skip, Abort };
    CbsServer& add_server(Task& task, Duration budget, Duration period,
                          OverrunPolicy overrun = OverrunPolicy::Queue);
    ```
    - Queue: new job queued, processed after current completes
    - Skip: new job dropped silently
    - Abort: current job aborted, new job starts

95. **Admission test on add_server().** Adding a server triggers admission
    test. Throws `AdmissionError` if utilization bound exceeded.
    - Uniprocessor CBS: `sum(U_i) <= 1`
    - Multiprocessor GRUB: `sum(U_i) <= m - (m-1) * U_max`
    Admission can be bypassed with `add_server_unchecked()` for research.

96. **Deadline miss: configurable policy.** Default handler logs the miss
    and continues execution. Researchers can override:
    ```cpp
    enum class DeadlineMissPolicy { Continue, AbortJob, AbortTask, StopSimulation };
    sched.set_deadline_miss_policy(DeadlineMissPolicy::Continue);
    // Or custom handler:
    sched.set_deadline_miss_handler([](Processor& p, Job& j) { /* custom */ });
    ```

97. **Single-threaded simulation, benchmark later.** No parallel simulation
    on the same engine. The main performance improvement comes from O(1)
    timer cancellation (iterator-based vs O(n) scan). Other improvements:
    streaming traces (no memory growth), O(1) server lookup. Benchmark
    after Phase 5 before further optimization.

98. **Platform JSON: explicit domains and processors.** New schema separates
    processor_types, clock_domains, power_domains, and processors:
    ```json
    {
      "processor_types": [
        {"name": "big", "performance": 1.0},
        {"name": "LITTLE", "performance": 0.3334}
      ],
      "clock_domains": [{
        "id": 0,
        "frequencies_mhz": [200, 400, 600, ...],
        "initial_frequency_mhz": 1200,
        "transition_delay_us": 100,
        "power_model": [a0, a1, a2, a3]
      }],
      "power_domains": [{
        "id": 0,
        "c_states": [
          {"level": 1, "power_mw": 20, "latency_us": 1, "scope": "per_processor"},
          {"level": 3, "power_mw": 2, "latency_us": 100, "scope": "domain_wide"}
        ]
      }],
      "processors": [
        {"id": 0, "type": "big", "clock_domain": 0, "power_domain": 0},
        ...
      ]
    }
    ```
    Library 3 provides backward-compatible loader for old cluster format.

99. **Scenario JSON: explicit wcet and relative_deadline.** Tasks specify
    `wcet` (in reference units) and `relative_deadline` instead of derived
    `utilization`. Job duration is in reference units:
    ```json
    {
      "tasks": [{
        "id": 0,
        "period": 10.0,
        "relative_deadline": 10.0,
        "wcet": 2.0,
        "jobs": [
          {"arrival": 0.0, "duration": 2.0},
          {"arrival": 10.0, "duration": 1.8}
        ]
      }]
    }
    ```
    Optional: server parameters can be specified per-task in scenario.

100. **Trace schema: consolidated state changes.** Replace multiple
     processor traces (ProcActivated, ProcIdled, ProcSleep, ProcChange)
     with single `proc_state_change` trace. Add new traces:
     - `deadline_miss` (task_id, proc_id)
     - `cstate_change` (proc_id, power_domain_id, level)
     - `task_migration` (task_id, from_proc, to_proc)
     All traces auto-timestamped by Library 1.

101. **Four-level testing strategy.**
     - **Level 1: Unit tests** (GoogleTest) — individual components, state
       machines, formulas. Target 90% coverage for Library 1.
     - **Level 2: Integration tests** (GoogleTest + fixtures) — Library 1 +
       Library 2 together, multi-processor scenarios, DVFS interactions.
     - **Level 3: Regression tests** (Python, trace checksums) — compare
       new simulator output against current simulator using verify_grub.py
       and golden checksums.
     - **Level 4: Property tests** (optional) — random scenario generation,
       verify invariants (energy monotonic, work conservation).

102. **Example programs for humans and LLMs.** 10 examples covering minimal
     simulation to custom scheduler/policy implementation. Each example:
     - `main.cpp` — complete, compilable, self-contained
     - `README.md` — explains concepts, links to API docs
     - Full imports and namespace usage (no implicit context)
     - Comments explaining "why" not just "what"
     Examples are designed to be easily parsed by LLMs for learning the API
     and generating new custom schedulers. Build with SCHEDSIM_BUILD_EXAMPLES.
     ```
     examples/
       01_minimal/           # Simplest simulation
       02_single_processor/  # EDF + CBS basics
       03_multiprocessor/    # Global EDF
       04_heterogeneous/     # big.LITTLE platform
       05_grub/              # GRUB reclamation
       06_dvfs/              # DVFS policies
       07_custom_scheduler/  # Subclassing Scheduler
       08_custom_dvfs/       # Custom DvfsPolicy
       09_trace_analysis/    # Library 3 post-processing
       10_scenario_gen/      # UUniFast generation
     ```

103. **Dual documentation: Doxygen + LLM-friendly markdown.** Two formats:
     - **Doxygen** — HTML API reference for human browsing. Generated from
       code comments. All public classes/methods documented with brief,
       params, return, throws, code examples.
     - **LLM reference** — Single markdown file `docs/llm-reference.md`
       consolidating API summaries, type signatures, usage patterns, and
       extension points. Designed for LLM context windows. Updated manually
       or generated from Doxygen XML output.
     ```
     docs/
       Doxyfile                # Doxygen config
       mainpage.md             # Doxygen landing page
       llm-reference.md        # Consolidated API for LLMs
       guides/
         getting_started.md
         concepts.md
         extending.md
       architecture.md         # This document
     ```

104. **Context-rich error messages.** Exceptions include operation name,
     expected/actual state, object identifiers, and values. Format:
     "Cannot <operation>() on <object> <id>: expected <X>, got <Y>".
     Examples:
     - "Cannot assign() on processor 3: expected Idle, got Running"
     - "Frequency 2500 MHz out of range [200, 2100] for clock domain 0"
     - "Cannot admit server: utilization 1.07 exceeds bound 1.0"
     Helper function `format_error()` ensures consistent formatting.

105. **CMake presets + simple workflow.** CMakePresets.json defines
     configurations (dev, release, sanitize, coverage). For users not
     familiar with presets, provide simple workflow:
     ```bash
     # Simple build (default: debug with tests)
     mkdir build && cd build
     cmake ..
     make        # or: cmake --build .
     ctest       # run tests

     # Release build
     cmake .. -DCMAKE_BUILD_TYPE=Release
     make
     ```
     Advanced users can use presets: `cmake --preset dev`. README documents
     both workflows. Default CMake configuration enables tests and examples.

106. **Code style: clang-format enforced.** Naming conventions:
     - Classes/Structs: `PascalCase` (Engine, CbsServer)
     - Functions/Methods: `snake_case` (add_timer, set_frequency)
     - Variables: `snake_case` (current_time, job_queue)
     - Member variables: `snake_case_` trailing underscore (processors_)
     - Constants: `UPPER_SNAKE_CASE` (MAX_PRIORITY)
     - Enum values: `PascalCase` (State::Running)
     - Files: `snake_case.hpp/cpp` (edf_scheduler.hpp)
     Format: 4-space indent, 100-char line limit, Attach braces. CI checks
     formatting with `clang-format --dry-run --Werror`.

107. **Directory structure finalized.** Three libraries under `schedsim/`:
     - `core/` — Library 1 (include/schedsim/core/, src/, tests/)
     - `algo/` — Library 2 (include/schedsim/algo/, src/, tests/)
     - `io/` — Library 3 (include/schedsim/io/, src/, tests/)
     Convenience headers: `<schedsim/core.hpp>`, `<schedsim/algo.hpp>`,
     `<schedsim/io.hpp>`. Examples in `examples/`. Docs in `docs/`.

108. **Dependencies via FetchContent with Nix fallback.** CMake uses
     `find_package()` first (works with Nix devShell), `FetchContent`
     fallback for users without Nix. Dependencies: GoogleTest, RapidJSON.
     No vendored code in repository.

109. **GitHub Actions CI with Nix.** CI uses Nix for reproducible builds:
     ```yaml
     jobs:
       build:
         runs-on: ubuntu-latest
         steps:
           - uses: actions/checkout@v4
           - uses: cachix/install-nix-action@v27
           - uses: cachix/cachix-action@v15
             with:
               name: schedsim  # Optional: Nix cache
           - run: nix develop --command bash -c "cmake -B build && cmake --build build"
           - run: nix develop --command bash -c "ctest --test-dir build"

       sanitizers:
         runs-on: ubuntu-latest
         steps:
           - uses: actions/checkout@v4
           - uses: cachix/install-nix-action@v27
           - run: nix develop --command bash -c "cmake -B build -DSCHEDSIM_ENABLE_SANITIZERS=ON && cmake --build build && ctest --test-dir build"

       format-check:
         runs-on: ubuntu-latest
         steps:
           - uses: actions/checkout@v4
           - uses: cachix/install-nix-action@v27
           - run: nix develop --command bash -c "find schedsim -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror"
     ```
     Jobs: build, sanitizers, coverage, regression tests, format check.

110. **`#pragma once` for header guards.** All headers use `#pragma once`
     instead of traditional include guards. Simpler, no collision risk,
     supported by all target compilers (GCC, Clang, Apple Clang).

---

## Open Questions

### Resolved

- ~~DVFS coordination~~ → Decision 76: lock_frequency() when shared.
- ~~Multiple clock domains in one scheduler~~ → Decision 77: not supported, throws.
- ~~Python/Rust wrappers~~ → Decision 78: C API layer, deferred to post-Phase 7.
- ~~Scheduler API exposed to allocator~~ → Decision 79: abstract Scheduler/Allocator interfaces.
- ~~DVFS policy interface~~ → Decision 84: DvfsPolicy abstract interface with delegation.
- ~~Power model refinement~~ → Decision 86: polynomial model + C-state power configuration.
- ~~Deferred callback ordering~~ → Decision 87: registration order, documented clearly.

### Remaining

None. All design questions resolved. Ready for implementation.

---

## Implementation Plan

### Phase 0: Project Setup

Set up the new project structure with three CMake targets:
```
schedsim/
  core/           ← Library 1: libschedsim-core
    include/
      schedsim/core/
        engine.hpp
        platform.hpp
        processor.hpp
        processor_type.hpp
        task.hpp
        job.hpp
        clock_domain.hpp
        power_domain.hpp
        timer.hpp
        trace.hpp
        time.hpp
        error.hpp
    src/
    tests/
  algo/           ← Library 2: libschedsim-algo
    include/
      schedsim/algo/
        scheduler.hpp           // Abstract Scheduler interface
        allocator.hpp           // Abstract Allocator interface
        edf_scheduler.hpp       // EdfScheduler implementation
        cbs_server.hpp          // CBS server state machine
        reclamation_policy.hpp  // ReclamationPolicy interface
        grub_policy.hpp         // GRUB implementation
        cash_policy.hpp         // CASH implementation
        dvfs_policy.hpp         // DvfsPolicy interface + built-ins
        dpm_policy.hpp          // DpmPolicy interface + built-ins
    src/
    tests/
  io/             ← Library 3: libschedsim-io
    include/
      schedsim/io/
        json_loader.hpp         // Platform/scenario loading
        trace_writer.hpp        // TraceWriter implementations
        task_generator.hpp      // UUniFast, synthetic workloads
        metrics.hpp             // Post-processing helpers
    src/
    tests/
```

Build system: CMake (primary). Nix flake for reproducible builds (optional).
C++ standard: C++20. Testing framework: GoogleTest (find_package + FetchContent
fallback). CI: works with or without Nix.

### Phase 1: Library 1 Core — Minimal Event Loop

Build the engine and time system:
1. **Time types**: `Duration` and `TimePoint` via `std::chrono::duration<double>`.
   Strong types for `Frequency`, `Power`, `Energy`.
2. **Engine**: event queue (`std::map<EventKey, Event>` with composite key),
   `run()` with three termination modes, `add_timer()` / `cancel_timer()`
3. **Deferred callbacks**: `register_deferred()` / `request_deferred()`
4. **ISR dispatch**: event priority ordering within a time step
5. **TraceWriter interface**: define the writer-callback API
6. **Error handling**: exception classes for API misuse, `assert()` for
   internal invariants

**Tests**: timer scheduling/cancellation, event ordering, deferred callback
semantics, termination modes.

### Phase 2: Library 1 — Platform Model

Build the hardware model:
1. **ProcessorType**: name + performance factor
2. **ClockDomain**: frequency range, transition delay, `set_frequency()`
3. **PowerDomain**: C-state levels (per-processor vs domain-wide),
   negotiation logic
4. **Processor**: state machine (Idle, Running, Sleep, Changing),
   `assign()` / `clear()` with fail-fast errors, ISR handler registration,
   clock/power domain association
5. **Platform**: factory for processor types, clock domains, power domains,
   processors. Declaration-order tracking.

**Tests**: processor state transitions, DVFS transitions (Changing → Available
ISR), C-state negotiation (per-processor vs domain-wide), error cases
(assign to sleeping processor, etc.).

### Phase 3: Library 1 — Task and Job Model

Build the workload model:
1. **Task**: period, relative deadline, WCET in reference units
2. **Job**: execution time in reference units, remaining time, absolute
   deadline. Job objects are core data; queue management is Library 2.
3. **Job Arrival ISR**: `schedule_job_arrival()` creates events, engine
   dispatches to global handler (allocator)
4. **Execution tracking**: normalized speed
   `(freq/freq_max) * (perf/perf_ref)`, remaining in reference units,
   fires Job Completion ISR at zero
5. **Deadline Miss ISR**: Library 1 sets internal timer at absolute
   deadline, fires ISR if job still pending
6. **Interaction with DVFS**: frequency change adjusts speed and
   recomputes completion time
7. **Interaction with preemption**: `proc.clear()` cancels pending
   completion event, updates remaining
8. **Migration support**: remaining is type-independent (reference units),
   re-assign to different processor type works automatically

**Tests**: execution tracking with constant frequency, execution tracking
with frequency change mid-job, preemption (assign → clear → re-assign),
migration across processor types, deadline miss detection.

### Phase 4: Library 1 — Optional Features

1. **Context switch overhead**: configurable delay on assign/clear
2. **Energy tracking**: power integration over time based on frequency,
   C-state, activity

**Tests**: context switch timing, energy computation with DVFS/DPM
transitions.

### Phase 5: Library 2 — Basic EDF Scheduler

Build a minimal EDF scheduler to validate the Library 1 API:
1. **Scheduler/Allocator interfaces**: abstract base classes (decision 79)
2. **CbsServer**: budget, period, virtual time, deadline, state machine,
   job queue with overrun policy (decision 94)
3. **EdfScheduler**: processor set, server list, deferred callback for
   dispatch, ISR handlers, policy composition (decision 89)
4. **EDF dispatch**: sort ready servers by deadline, assign to processors
5. **CBS budget timer**: schedule closure, handle budget exhaustion
6. **Basic allocator**: SingleSchedulerAllocator
7. **Admission test**: on add_server(), throws AdmissionError (decision 95)
8. **Deadline miss handler**: configurable policy (decision 96)

**Tests**: single-processor EDF (compare against known results),
multi-processor global EDF, CBS budget exhaustion and replenishment,
admission test, overrun policies, deadline miss handling.

### Phase 6: Library 2 — Reclamation and Power Policies

1. **ReclamationPolicy interface**: extension points (decision 90)
2. **GrubPolicy**: NonContending state, modified VT formula, active bandwidth
3. **CashPolicy**: spare queue implementation
4. **DvfsPolicy implementations**: PowerAware, CSF, FFA (decision 84)
5. **Timer policy variants**: cooldown-based DVFS (decision 88)
6. **DpmPolicy implementations**: C-state management (decision 85)

**Verification**: compare traces against current simulator output using
`verify_grub.py` test suite. Test GRUB, CASH reclamation correctness.

### Phase 7: Library 3 — I/O

1. **JSON platform loader**: read platform description (with C-states)
2. **JSON scenario loader**: read task set and job arrivals
3. **JsonTraceWriter**: streaming JSON output
4. **MemoryTraceWriter**: in-memory buffer
5. **NullTraceWriter**: no-op for performance
6. **Scenario generation**: UUniFast, task set generation, job arrival
   generation (decision 92)
7. **Metrics helpers**: compute statistics from traces (deadline misses,
   preemptions, energy, utilization)

### Phase 8: Validation

Run the full existing test suite against the new architecture. Compare
trace output checksums. The new simulator must produce identical scheduling
decisions for the same inputs.
