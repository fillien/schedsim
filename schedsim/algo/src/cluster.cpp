#include <schedsim/algo/cluster.hpp>

#include <schedsim/algo/scheduler.hpp>

#include <schedsim/core/clock_domain.hpp>

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

} // namespace schedsim::algo
