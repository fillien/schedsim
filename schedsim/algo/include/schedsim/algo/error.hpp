#pragma once

#include <stdexcept>
#include <string>

namespace schedsim::algo {

// Exception thrown when a CBS server cannot be admitted due to utilization constraints
class AdmissionError : public std::runtime_error {
public:
    AdmissionError(double requested_utilization, double available_capacity)
        : std::runtime_error(
              "Cannot admit server: requested utilization " +
              std::to_string(requested_utilization) +
              " exceeds available capacity " +
              std::to_string(available_capacity))
        , requested_(requested_utilization)
        , available_(available_capacity) {}

    [[nodiscard]] double requested_utilization() const noexcept { return requested_; }
    [[nodiscard]] double available_capacity() const noexcept { return available_; }

private:
    double requested_;
    double available_;
};

// Admission test for CBS server admission
enum class AdmissionTest {
    CapacityBound,  // U <= m  (necessary condition, default)
    GFB             // U <= m - (m-1)*u_max  (sufficient, Goossens-Funk-Baruah 2003)
};

// Policy for handling deadline misses
enum class DeadlineMissPolicy {
    Continue,       // Log and continue (default)
    AbortJob,       // Abort the job, keep task
    AbortTask,      // Remove task from scheduler
    StopSimulation  // Halt simulation
};

} // namespace schedsim::algo
