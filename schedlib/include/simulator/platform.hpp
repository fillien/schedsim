#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "simulator/entity.hpp"
#include "simulator/processor.hpp"

#include <cstddef>
#include <memory>
#include <vector>

class Timer;

namespace scheds {
class Scheduler;
}

/**
 * @brief Represents a cluster in a platform.
 */
class Cluster : public Entity {
      public:
        static constexpr double DVFS_DELAY{0.5};

        explicit Cluster(
            Engine& sim,
            std::size_t cid,
            const std::vector<double>& frequencies,
            const double& effective_freq,
            const double& perf_score,
            const double& u_target);

        ~Cluster();

        [[nodiscard]] auto freq_max() const -> double { return *frequencies_.begin(); }
        [[nodiscard]] auto freq_min() const -> double { return *frequencies_.rbegin(); }
        [[nodiscard]] auto freq_eff() const -> double { return effective_freq_; }
        [[nodiscard]] auto freq() const -> double { return current_freq_; }
        [[nodiscard]] auto speed() const -> double;
        [[nodiscard]] auto perf() const -> double { return perf_score_; }
        [[nodiscard]] auto scale_speed() const -> double;
        [[nodiscard]] auto id() const -> std::size_t { return id_; }
        [[nodiscard]] auto u_target() const -> double { return u_target_; }
        void u_target(const double& target) { u_target_ = target; }

        auto ceil_to_mode(const double& freq) -> double;
        void dvfs_change_freq(const double& next_freq);
        void create_procs(std::size_t nb_procs);

        [[nodiscard]] auto scheduler() const -> scheds::Scheduler* { return attached_scheduler_; }
        void scheduler(scheds::Scheduler* sched) { attached_scheduler_ = sched; }

        [[nodiscard]] auto processors() const -> const std::vector<std::unique_ptr<Processor>>&
        {
                return processors_;
        }

      private:
        void freq(const double& new_freq);

        std::vector<std::unique_ptr<Processor>> processors_;
        std::size_t id_;
        std::vector<double> frequencies_;
        double effective_freq_;
        double current_freq_;
        double perf_score_;
        double u_target_;
        std::unique_ptr<Timer> dvfs_timer_;
        double dvfs_target_{0};
        scheds::Scheduler* attached_scheduler_{nullptr};
};

/**
 * @brief Represents a platform containing multiple clusters.
 */
class Platform : public Entity {
      public:
        explicit Platform(Engine& sim, bool freescaling_allowed)
            : Entity(sim), freescaling_(freescaling_allowed)
        {
        }

        [[nodiscard]] auto is_freescaling() const -> bool { return freescaling_; }
        auto reserve_next_id() -> std::size_t { return cpt_id_++; }

        [[nodiscard]] auto clusters() const -> const std::vector<std::unique_ptr<Cluster>>&
        {
                return clusters_;
        }

        void add_cluster(std::unique_ptr<Cluster> new_cluster)
        {
                clusters_.push_back(std::move(new_cluster));
        }

      private:
        std::vector<std::unique_ptr<Cluster>> clusters_;
        bool freescaling_;
        std::size_t cpt_id_{1};
};

#endif
