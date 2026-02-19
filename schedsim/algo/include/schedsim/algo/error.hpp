#pragma once

#include <stdexcept>
#include <string>

namespace schedsim::algo {

/// @brief Exception thrown when a CBS server cannot be admitted.
/// @ingroup algo
///
/// Raised by Scheduler::on_job_arrival or Allocator::on_job_arrival when
/// the requested server utilization would exceed the available scheduling
/// capacity.
///
/// @see Scheduler::can_admit, AdmissionTest
class AdmissionError : public std::runtime_error {
public:
    /// @brief Construct an AdmissionError with utilization details.
    ///
    /// @param requested_utilization The utilization that was requested.
    /// @param available_capacity    The remaining capacity at the time of the request.
    AdmissionError(double requested_utilization, double available_capacity)
        : std::runtime_error(
              "Cannot admit server: requested utilization " +
              std::to_string(requested_utilization) +
              " exceeds available capacity " +
              std::to_string(available_capacity))
        , requested_(requested_utilization)
        , available_(available_capacity) {}

    /// @brief Get the utilization that was requested.
    /// @return Requested utilization as a dimensionless ratio.
    [[nodiscard]] double requested_utilization() const noexcept { return requested_; }

    /// @brief Get the capacity that was available at the time of the request.
    /// @return Available capacity as a dimensionless value.
    [[nodiscard]] double available_capacity() const noexcept { return available_; }

private:
    double requested_;
    double available_;
};

/// @brief Admission test variant for CBS server admission control.
///
/// Selects the schedulability condition used when admitting a new server
/// to a global EDF scheduler.
///
/// @see Scheduler::can_admit
enum class AdmissionTest {
    CapacityBound,  ///< U <= m (necessary condition, default).
    GFB             ///< U <= m - (m-1)*u_max (sufficient, Goossens-Funk-Baruah 2003).
};

/// @brief Policy for handling scheduling deadline misses.
///
/// Controls simulator behaviour when a job misses its absolute deadline.
///
/// @see EdfScheduler, DeadlineMissPolicy
enum class DeadlineMissPolicy {
    Continue,       ///< Log the miss and continue simulation (default).
    AbortJob,       ///< Abort the offending job but keep the task active.
    AbortTask,      ///< Remove the entire task from the scheduler.
    StopSimulation  ///< Halt the simulation immediately.
};

} // namespace schedsim::algo
