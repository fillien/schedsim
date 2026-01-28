#pragma once

#include <stdexcept>
#include <string>

namespace schedsim::core {

// Base exception for all simulation errors (decision 74)
class SimulationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown when an operation is invalid for current state
class InvalidStateError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

// Thrown when a value is out of valid range
class OutOfRangeError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

// Thrown when trying to modify after finalize()
class AlreadyFinalizedError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

// Thrown when setting a handler that's already set
class HandlerAlreadySetError : public SimulationError {
public:
    using SimulationError::SimulationError;
};

} // namespace schedsim::core
