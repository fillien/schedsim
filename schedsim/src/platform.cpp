#include <algorithm>
#include <cassert>
#include <cstddef>
#include <engine.hpp>
#include <functional>
#include <iterator>
#include <memory>
#include <platform.hpp>
#include <processor.hpp>
#include <protocols/traces.hpp>
#include <vector>

Cluster::Cluster(
    const std::weak_ptr<Engine>& sim,
    const std::size_t cid,
    const std::vector<double>& frequencies,
    const double& effective_freq,
    const double& perf_score)
    : Entity(sim), id_(cid), frequencies_(std::move(frequencies)), effective_freq_(effective_freq),
      current_freq_(0), perf_score_(perf_score)
{
        assert(std::ranges::all_of(frequencies_, [](double freq) { return freq > 0; }));

        assert(std::ranges::any_of(
            frequencies_, [this](double freq) { return freq == effective_freq_; }));

        std::ranges::sort(frequencies_, std::greater<>());

        freq(freq_max());

        dvfs_timer_ = std::make_shared<Timer>(sim, [this]() { freq(dvfs_target_); });
}

void Cluster::create_procs(const std::size_t nb_procs)
{
        assert(nb_procs > 0);
        processors_.reserve(nb_procs);

        for (std::size_t i = 0; i < nb_procs; ++i) {
                const auto proc_id = sim()->chip()->reserve_next_id();
                processors_.push_back(
                    std::make_shared<Processor>(sim(), shared_from_this(), proc_id));
        }
}

void Cluster::freq(const double& new_freq)
{
        if (new_freq < 0 || new_freq > freq_max()) {
                throw std::domain_error("This frequency is not available");
        }

        const bool free_scaling = sim()->chip()->is_freescaling();
        const double target_freq = free_scaling ? new_freq : ceil_to_mode(new_freq);

        if (current_freq_ != target_freq) {
                current_freq_ = target_freq;
                sim()->add_trace(protocols::traces::FrequencyUpdate{
                    .cluster_id = id_, .frequency = current_freq_});
        }
}

auto Cluster::ceil_to_mode(const double& freq) -> double
{
        assert(!frequencies_.empty());
        const auto itr = std::ranges::lower_bound(frequencies_, freq, std::greater<>());
        if (itr == frequencies_.begin()) { return *itr; }
        if (itr == frequencies_.end()) { return freq_min(); }
        return *std::prev(itr);
}

void Cluster::dvfs_change_freq(const double& next_freq)
{
        using enum Processor::State;

        if (next_freq < 0 || next_freq > freq_max()) {
                throw std::domain_error("This frequency is not available");
        }

        const bool free_scaling = sim()->chip()->is_freescaling();
        const double target_freq = free_scaling ? next_freq : ceil_to_mode(next_freq);
        if (target_freq == current_freq_) { return; }

        if (!sim()->is_delay_activated()) {
                freq(next_freq);
                return;
        }

        if (!dvfs_timer_->is_active()) {
                dvfs_target_ = target_freq;
                for (auto& proc : processors_) {
                        proc->dvfs_change_state(DVFS_DELAY);
                }
                dvfs_timer_->set(DVFS_DELAY);
        }
        else {
                for (const auto& proc : processors_) {
                        assert(proc->state() == Processor::State::Change);
                }
        }
}
