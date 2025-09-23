#include <iterator>
#include <print>
#include <simulator/allocator.hpp>
#include <simulator/allocators/ff_sma.hpp>
#include <simulator/scheduler.hpp>

#include <algorithm>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

allocators::FFSma::FFSma(const std::weak_ptr<Engine>& sim, double sample_rate, int num_samples)
    : Allocator(sim), sample_rate_(sample_rate), num_samples_(num_samples)
{
        if (sample_rate_ <= 0.0) {
                throw std::invalid_argument("FFSma: sample_rate must be strictly positive");
        }
        if (num_samples_ <= 0) {
                throw std::invalid_argument("FFSma: num_samples must be strictly positive");
        }
}

auto computeSMA(
    const std::vector<std::pair<double, double>>& data, double sample_rate, int num_samples)
    -> double
{
        if (data.empty()) { return 0.0; }

        double tn = data.back().first;
        double T = static_cast<double>(num_samples) / sample_rate;
        double t_start = tn - T;

        auto it = std::lower_bound(
            data.begin(), data.end(), t_start, [](const std::pair<double, double>& p, double t) {
                    return p.first < t;
            });

        double sum = std::accumulate(
            it, data.end(), 0.0, [](double acc, const std::pair<double, double>& p) {
                    return acc + p.second;
            });

        size_t count = std::distance(it, data.end());
        return (count == 0) ? 0.0 : sum / static_cast<double>(count);
}

auto allocators::FFSma::where_to_put_the_task(const std::shared_ptr<Task>& new_task)
    -> std::optional<std::shared_ptr<scheds::Scheduler>>
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        auto sorted_scheds{schedulers()};
        std::ranges::sort(sorted_scheds, compare_perf);

        std::optional<std::shared_ptr<scheds::Scheduler>> next_sched;

        const double NB_PROCS =
            static_cast<double>(sorted_scheds.back()->cluster()->processors().size());

        // Look for a cluster to place the task
        for (auto& sched : sorted_scheds) {
                const auto& clu = sched->cluster();

                if (sched != sorted_scheds.back()) {
                        sched->cluster()->u_target(
                            computeSMA(
                                sorted_scheds.back()->last_utilizations(),
                                sample_rate_,
                                num_samples_) /
                            NB_PROCS);
                }

                if (sched != sorted_scheds.back()) { clu->u_target(); }
                if (((new_task->utilization() * clu->scale_speed()) / clu->perf()) <
                    clu->u_target()) {
                        if (sched->admission_test(*new_task)) {
                                next_sched = sched;
                                break;
                        }
                }
        }

        // Otherwise return that the allocator didn't found a cluster to place the task
        return next_sched;
}
