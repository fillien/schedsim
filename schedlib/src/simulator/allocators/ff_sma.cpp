#include <iterator>
#include <print>
#include <simulator/allocator.hpp>
#include <simulator/allocators/ff_sma.hpp>
#include <simulator/scheduler.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

allocators::FFSma::FFSma(Engine& sim, double sample_rate, int num_samples)
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

        const double tn = data.back().first;
        const double window = static_cast<double>(num_samples) / sample_rate;
        const double t_start = tn - window;

        if (window <= 0.0) { return 0.0; }

        const auto lower = std::lower_bound(
            data.begin(),
            data.end(),
            t_start,
            [](const std::pair<double, double>& sample, double value) {
                    return sample.first < value;
            });

        double last_time = t_start;
        double last_value = data.back().second;

        if (lower == data.begin()) {
                if (lower != data.end()) { last_value = lower->second; }
        }
        else if (lower == data.end()) {
                last_value = data.back().second;
        }
        else {
                const auto prev = std::prev(lower);
                const double t0 = prev->first;
                const double v0 = prev->second;
                const double t1 = lower->first;
                const double v1 = lower->second;
                if (std::abs(t1 - t0) > std::numeric_limits<double>::epsilon()) {
                        const double alpha = (t_start - t0) / (t1 - t0);
                        last_value = v0 + alpha * (v1 - v0);
                }
                else {
                        last_value = v1;
                }
        }

        double integral = 0.0;

        auto it = lower;
        for (; it != data.end() && it->first <= tn; ++it) {
                const double current_time = std::clamp(it->first, t_start, tn);
                const double current_value = it->second;
                const double dt = current_time - last_time;
                if (dt > 0.0) {
                        integral += 0.5 * (last_value + current_value) * dt;
                        last_time = current_time;
                        last_value = current_value;
                }
                else {
                        last_value = current_value;
                        last_time = current_time;
                }
        }

        if (last_time < tn) {
                const double dt = tn - last_time;
                integral += 0.5 * (last_value + data.back().second) * dt;
        }

        return integral / window;
}

auto allocators::FFSma::where_to_put_the_task(const Task& new_task) -> scheds::Scheduler*
{
        const auto compare_perf = [](const auto& first, const auto& second) {
                return first->cluster()->perf() < second->cluster()->perf();
        };

        // Sort the schedulers by perf score
        std::vector<scheds::Scheduler*> sorted_scheds;
        sorted_scheds.reserve(schedulers().size());
        for (const auto& s : schedulers()) sorted_scheds.push_back(s.get());
        std::ranges::sort(sorted_scheds, compare_perf);

        const double NB_PROCS =
            static_cast<double>(sorted_scheds.back()->cluster()->processors().size());

        // Look for a cluster to place the task
        for (auto* sched : sorted_scheds) {
                const auto* clu = sched->cluster();

                if (sched != sorted_scheds.back()) {
                        sched->cluster()->u_target(
                            computeSMA(
                                sorted_scheds.back()->last_utilizations(),
                                sample_rate_,
                                num_samples_) /
                            NB_PROCS);
                }

                if (((new_task.utilization() * clu->scale_speed()) / clu->perf()) <
                    clu->u_target()) {
                        if (sched->admission_test(new_task)) { return sched; }
                }
        }

        // Otherwise return that the allocator didn't find a cluster to place the task
        return nullptr;
}
