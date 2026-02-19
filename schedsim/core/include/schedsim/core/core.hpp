#pragma once

/// @defgroup core Core Library
/// @brief Simulation engine, hardware model, events, and types.
///
/// The core library provides the foundational simulation infrastructure:
/// the event-driven Engine, the Platform hardware model (processors,
/// clock domains, power domains), tasks, jobs, timers, and deferred
/// callbacks. It has no dependencies on scheduling algorithms or I/O.

/// @defgroup core_types Types
/// @ingroup core
/// @brief Strong types for time, frequency, power, and energy.

/// @defgroup core_engine Engine
/// @ingroup core
/// @brief Event-driven simulation loop and timer API.

/// @defgroup core_hardware Hardware Model
/// @ingroup core
/// @brief Platform, Processor, ClockDomain, PowerDomain, ProcessorType.

/// @defgroup core_events Events
/// @ingroup core
/// @brief Event types, timer identifiers, and deferred callbacks.

// Convenience header for Library 1
#include <schedsim/core/types.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/event.hpp>
#include <schedsim/core/timer.hpp>
#include <schedsim/core/deferred.hpp>
#include <schedsim/core/trace_writer.hpp>

// Phase 2: Platform model
#include <schedsim/core/processor_type.hpp>
#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/task.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/processor.hpp>
#include <schedsim/core/platform.hpp>

#include <schedsim/core/engine.hpp>
