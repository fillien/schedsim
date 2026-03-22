#include <schedsim/algo/cluster.hpp>

#include <schedsim/algo/scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace schedsim::algo {

Cluster::Cluster(core::ClockDomain& clock_domain, Scheduler& scheduler,
                 double perf_score, double reference_freq_max)
    : clock_domain_(clock_domain)
    , scheduler_(scheduler)
    , perf_score_(perf_score)
    , reference_freq_max_(reference_freq_max) {}

double Cluster::scale_speed() const noexcept {
    double local_max = clock_domain_.freq_max().mhz;
    if (local_max <= 0.0) {
        return 1.0;
    }
    return reference_freq_max_ / local_max;
}

double Cluster::scaled_utilization(double task_util) const noexcept {
    if (perf_score_ <= 0.0) {
        return task_util;
    }
    return task_util * scale_speed() / perf_score_;
}

void Cluster::set_processor_id(std::size_t id) noexcept {
    processor_id_ = id;
}

std::optional<std::size_t> Cluster::processor_id() const noexcept {
    return processor_id_;
}

double Cluster::remaining_capacity() const noexcept {
    return static_cast<double>(processor_count()) - utilization();
}

std::size_t Cluster::processor_count() const noexcept {
    return scheduler_.processor_count();
}

double Cluster::utilization() const noexcept {
    return scheduler_.utilization();
}

bool Cluster::can_admit(core::Duration budget, core::Duration period) const {
    return scheduler_.can_admit(budget, period);
}

bool Cluster::try_admit_scaled(double task_util) {
    constexpr double epsilon = 1e-9;
    double scaled = scaled_utilization(task_util);

    // u_target check
    if (scaled > u_target_ + epsilon) {
        return false;
    }

    // GFB check with scaled utilization: capacity = m - (m-1)*u_max
    auto m = static_cast<double>(processor_count());
    double u_max = std::max(max_scaled_utilization_, scaled);
    double capacity = (m > 1.0) ? m - (m - 1.0) * u_max : m;
    if (total_scaled_utilization_ + scaled > capacity + epsilon) {
        return false;
    }

    // Admit: update tracking
    total_scaled_utilization_ += scaled;
    max_scaled_utilization_ = u_max;
    admitted_scaled_utils_.push_back(scaled);
    return true;
}

void Cluster::unadmit_scaled(double task_util) {
    double scaled = scaled_utilization(task_util);
    total_scaled_utilization_ -= scaled;
    if (total_scaled_utilization_ < 0.0) {
        total_scaled_utilization_ = 0.0;
    }

    // Find and erase the matching entry (epsilon-based for floating-point safety)
    auto it = std::find_if(admitted_scaled_utils_.begin(), admitted_scaled_utils_.end(),
                           [scaled](double v) { return std::abs(v - scaled) < 1e-12; });
    if (it != admitted_scaled_utils_.end()) {
        admitted_scaled_utils_.erase(it);
    }

    // Recompute max from remaining entries
    if (admitted_scaled_utils_.empty()) {
        max_scaled_utilization_ = 0.0;
    } else {
        max_scaled_utilization_ = *std::max_element(
            admitted_scaled_utils_.begin(), admitted_scaled_utils_.end());
    }
}

void Cluster::readmit_scaled(double task_util) {
    double scaled = scaled_utilization(task_util);
    total_scaled_utilization_ += scaled;
    admitted_scaled_utils_.push_back(scaled);
    if (scaled > max_scaled_utilization_) {
        max_scaled_utilization_ = scaled;
    }
}

} // namespace schedsim::algo
