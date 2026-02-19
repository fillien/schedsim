#pragma once

#include <stdexcept>
#include <string>

namespace schedsim::core {

/// @brief Base exception for all simulation errors.
///
/// All exceptions thrown by the core library derive from this class,
/// allowing callers to catch simulation-specific errors separately
/// from other `std::runtime_error` exceptions.
///
/// @see InvalidStateError, OutOfRangeError, AlreadyFinalizedError, HandlerAlreadySetError
/// @ingroup core
class SimulationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// @brief Thrown when an operation is invalid for the current object state.
///
/// For example, calling a method that requires finalization before the
/// engine has been finalized, or attempting to execute a job on a
/// processor that is already busy.
///
/// @see SimulationError
/// @ingroup core
class InvalidStateError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

/// @brief Thrown when a value is outside its valid range.
///
/// For example, requesting a processor by index when the index
/// exceeds the number of processors in the platform.
///
/// @see SimulationError
/// @ingroup core
class OutOfRangeError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

/// @brief Thrown when attempting to modify the platform after finalize().
///
/// Once Engine::finalize() has been called, hardware and task
/// collections are locked. Any add_* call after that point raises
/// this exception.
///
/// @see Engine::finalize, SimulationError
/// @ingroup core
class AlreadyFinalizedError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

/// @brief Thrown when setting a callback handler that has already been set.
///
/// Handlers such as Engine::set_job_arrival_handler() may only be
/// registered once. A second call raises this exception.
///
/// @see Engine::set_job_arrival_handler, SimulationError
/// @ingroup core
class HandlerAlreadySetError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

} // namespace schedsim::core
