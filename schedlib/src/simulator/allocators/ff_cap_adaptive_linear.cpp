#include <simulator/allocators/ff_cap_adaptive_linear.hpp>

#include <simulator/scheduler.hpp>

#include <algorithm>
#include <vector>

namespace {

// Linear model: target = A_UMAX*umax + B_U*U + C
// R² = 0.9644
constexpr double A_UMAX = 1.616;
constexpr double B_U = 0.098;
constexpr double C = -0.373;

// Manual offset to adjust threshold (tuning parameter)
constexpr double OFFSET = 0.0;

auto clamp01(double x) -> double
{
        return std::clamp(x, 0.0, 1.0);
}

auto model_target(double umax, double U) -> double
{
        // Linear: A_UMAX*umax + B_U*U + C + OFFSET
        const double result = (A_UMAX * umax) + (B_U * U) + C + OFFSET;
        return clamp01(result);
}

} // namespace

auto allocators::FFCapAdaptiveLinear::where_to_put_the_task(const Task& new_task)
    -> scheds::Scheduler*
{
        // Update observed umax with incoming task's utilization
        observed_umax_ = std::max(observed_umax_, new_task.utilization());

        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        std::vector<scheds::Scheduler*> sorted_scheds;
        sorted_scheds.reserve(schedulers().size());
        for (const auto& s : schedulers()) sorted_scheds.push_back(s.get());
        std::ranges::sort(sorted_scheds, compare_perf);

        if (!sorted_scheds.empty()) {
                // Use observed umax from incoming tasks
                const double target = model_target(observed_umax_, expected_total_util_);
                sorted_scheds.front()->cluster()->u_target(target);
        }

        for (auto* sched : sorted_scheds) {
                const auto* clu = sched->cluster();
                const double scaled_utilization =
                    (new_task.utilization() * clu->scale_speed()) / clu->perf();

                if (scaled_utilization <= clu->u_target()) {
                        if (sched->admission_test(new_task)) { return sched; }
                }
        }

        return nullptr;
}
