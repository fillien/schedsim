#include <simulator/allocators/ff_cap_adaptive_poly.hpp>

#include <simulator/scheduler.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>

namespace {

// Polynomial model coefficients: target = C0 + C1*umax + C2*U + C3*umax² + C4*umax*U + C5*U²
// R² = 0.9757, MAE = 0.0275
constexpr double C0 = -0.285854319;
constexpr double C1 = 2.339707990;
constexpr double C2 = 0.031898477;
constexpr double C3 = -1.376401346;
constexpr double C4 = -0.037369647;
constexpr double C5 = 0.007632732;

auto clamp01(double x) -> double
{
        return std::clamp(x, 0.0, 1.0);
}

auto model_target(double umax, double U) -> double
{
        const double u2 = umax * umax;
        const double uU = umax * U;
        const double U2 = U * U;
        return clamp01(C0 + (C1 * umax) + (C2 * U) + (C3 * u2) + (C4 * uU) + (C5 * U2));
}

} // namespace

auto allocators::FFCapAdaptivePoly::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        // Update observed umax with incoming task's utilization
        observed_umax_ = std::max(observed_umax_, new_task->utilization());

        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        auto sorted_scheds{schedulers()};
        std::ranges::sort(sorted_scheds, compare_perf);

        if (!sorted_scheds.empty()) {
                // Use expected total utilization (set from taskset), not current utilization
                const double target = model_target(observed_umax_, expected_total_util_);
                sorted_scheds.front()->cluster()->u_target(target);
        }

        std::optional<std::shared_ptr<scheds::Scheduler>> next_sched;

        for (auto& sched : sorted_scheds) {
                const auto& clu = sched->cluster();
                const double scaled_utilization =
                    (new_task->utilization() * clu->scale_speed()) / clu->perf();

                if (scaled_utilization <= clu->u_target()) {
                        if (sched->admission_test(*new_task)) {
                                next_sched = sched;
                                break;
                        }
                }
        }

        return next_sched;
}
