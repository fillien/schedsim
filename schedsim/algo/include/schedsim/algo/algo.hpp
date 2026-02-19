#pragma once

/// @defgroup algo Algo Library
/// @brief Schedulers, CBS servers, reclamation/DVFS/DPM policies, and allocators.
///
/// The algo library implements scheduling algorithms on top of the core
/// simulation engine. It provides the EDF scheduler with CBS bandwidth
/// servers, GRUB/CASH reclamation policies, DVFS frequency scaling
/// policies (PA, FFA, CSF), DPM power management, and multi-cluster
/// task allocators. Depends on core only.

/// @defgroup algo_schedulers Schedulers
/// @ingroup algo
/// @brief EDF scheduler and abstract Scheduler interface.

/// @defgroup algo_servers CBS Servers
/// @ingroup algo
/// @brief Constant Bandwidth Server implementation.

/// @defgroup algo_policies Policies
/// @ingroup algo
/// @brief Reclamation, DVFS, and DPM policies.

/// @defgroup algo_allocators Allocators
/// @ingroup algo
/// @brief Task-to-cluster allocation strategies.

// Convenience header for Library 2 (libschedsim-algo)
// Includes all public headers for scheduling algorithms

#include <schedsim/algo/allocator.hpp>
#include <schedsim/algo/cbs_server.hpp>
#include <schedsim/algo/edf_scheduler.hpp>
#include <schedsim/algo/error.hpp>
#include <schedsim/algo/scheduler.hpp>
#include <schedsim/algo/single_scheduler_allocator.hpp>

namespace schedsim::algo {
// All types are defined in their respective headers
}
