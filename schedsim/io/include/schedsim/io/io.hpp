#pragma once

/// @defgroup io I/O Library
/// @brief JSON loading, trace output, scenario generation, and metrics.
///
/// The I/O library handles all external data formats: loading platform
/// and scenario JSON files, writing simulation traces (JSON, textual,
/// in-memory), computing post-simulation metrics (response times,
/// deadline misses, energy), and generating random task sets.
/// Depends on core only.

/// @defgroup io_loaders Loaders
/// @ingroup io
/// @brief Platform and scenario JSON loaders.

/// @defgroup io_writers Trace Writers
/// @ingroup io
/// @brief JSON, textual, memory, and null trace writers.

/// @defgroup io_metrics Metrics
/// @ingroup io
/// @brief Post-simulation metrics and time-series extraction.

/// @defgroup io_generation Generation
/// @ingroup io
/// @brief Random scenario generation (UUniFast-Discard).

// Convenience header for Library 3 (I/O)

#include <schedsim/io/error.hpp>
#include <schedsim/io/trace_writers.hpp>
#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/scenario_injection.hpp>
#include <schedsim/io/scenario_generation.hpp>
#include <schedsim/io/metrics.hpp>
