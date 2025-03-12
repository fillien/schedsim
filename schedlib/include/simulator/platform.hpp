#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "simulator/entity.hpp"
#include "simulator/processor.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace scheds {
class Scheduler;
}

/**
 * @brief Represents a cluster in a platform.
 *
 * A Cluster manages a group of processors and handles dynamic voltage and frequency scaling (DVFS).
 */
class Cluster : public Entity, public std::enable_shared_from_this<Cluster> {
      public:
        /// Delay used for DVFS transitions.
        static constexpr double DVFS_DELAY{0.5};

        /**
         * @brief Constructs a new Cluster instance.
         *
         * @param sim A weak pointer to the engine.
         * @param id The identifier for the cluster.
         * @param frequencies Available frequency modes.
         * @param effective_freq The effective frequency.
         * @param perf_score The performance score.
         */
        explicit Cluster(
            const std::weak_ptr<Engine>& sim,
            const std::size_t cid,
            const std::vector<double>& frequencies,
            const double& effective_freq,
            const double& perf_score,
            const double& u_target);

        /**
         * @brief Returns the maximum available frequency.
         * @return The maximum frequency.
         */
        [[nodiscard]] auto freq_max() const -> double { return *frequencies_.begin(); }

        /**
         * @brief Returns the minimum available frequency.
         * @return The minimum frequency.
         */
        [[nodiscard]] auto freq_min() const -> double { return *frequencies_.rbegin(); }

        /**
         * @brief Returns the effective frequency.
         * @return The effective frequency.
         */
        [[nodiscard]] auto freq_eff() const -> double { return effective_freq_; }

        /**
         * @brief Gets the current operating frequency.
         * @return The current frequency.
         */
        [[nodiscard]] auto freq() const -> double { return current_freq_; }

        /**
         * @brief Calculates the relative speed with respect to the maximum frequency.
         * @return The relative speed.
         */
        [[nodiscard]] auto speed() const -> double { return current_freq_ / freq_max(); }

        /**
         * @brief Returns the performance score.
         * @return The performance score.
         */
        [[nodiscard]] auto perf() const -> double { return perf_score_; }

        /**
         * @brief Returns the cluster identifier.
         * @return The cluster id.
         */
        [[nodiscard]] auto id() const -> std::size_t { return id_; }

        [[nodiscard]] auto u_target() const -> double { return u_target_; }

        /**
         * @brief Rounds a given frequency up to the nearest available mode.
         * @param freq The frequency to round up.
         * @return The nearest available frequency mode.
         */
        auto ceil_to_mode(const double& freq) -> double;

        /**
         * @brief Changes the frequency for DVFS.
         * @param next_freq The target frequency.
         */
        void dvfs_change_freq(const double& next_freq);

        /**
         * @brief Creates a specified number of processors in the cluster.
         * @param nb_procs The number of processors to create.
         */
        void create_procs(const std::size_t nb_procs);

        /**
         * @brief Retrieves the scheduler attached to this cluster.
         * @return A weak pointer to the scheduler.
         */
        [[nodiscard]] auto scheduler() const -> std::weak_ptr<scheds::Scheduler>
        {
                return attached_scheduler_;
        }

        /**
         * @brief Attaches a scheduler to this cluster.
         * @param sched A weak pointer to the scheduler.
         */
        void scheduler(const std::weak_ptr<scheds::Scheduler>& sched)
        {
                attached_scheduler_ = sched;
        }

        [[nodiscard]] auto processors() const -> const std::vector<std::shared_ptr<Processor>>&
        {
                return processors_;
        }

      private:
        /**
         * @brief Sets the current operating frequency.
         * @param new_freq The new frequency.
         */
        void freq(const double& new_freq);

        std::vector<std::shared_ptr<Processor>> processors_;
        std::size_t id_;
        std::vector<double> frequencies_;
        double effective_freq_;
        double current_freq_;
        double perf_score_;
        double u_target_;
        std::shared_ptr<Timer> dvfs_timer_;
        double dvfs_target_{0};
        std::weak_ptr<scheds::Scheduler> attached_scheduler_;
};

/**
 * @brief Represents a platform containing multiple clusters.
 *
 * A Platform aggregates clusters and provides resource management for an entire system, such as an
 * SoC.
 */
class Platform : public Entity {
      public:
        /**
         * @brief Constructs a new Platform instance.
         *
         * @param sim A weak pointer to the engine.
         * @param freescaling_allowed Indicates whether free-scaling is enabled.
         */
        explicit Platform(const std::weak_ptr<Engine>& sim, bool freescaling_allowed)
            : Entity(sim), freescaling_(freescaling_allowed)
        {
        }

        /**
         * @brief Checks whether free-scaling is enabled.
         * @return True if free-scaling is enabled, false otherwise.
         */
        [[nodiscard]] auto is_freescaling() const -> bool { return freescaling_; }

        /**
         * @brief Reserves and returns the next available identifier.
         * @return The next available identifier.
         */
        auto reserve_next_id() -> std::size_t { return cpt_id_++; }

        [[nodiscard]] auto clusters() const -> const std::vector<std::shared_ptr<Cluster>>&
        {
                return clusters_;
        }

        void add_cluster(const std::shared_ptr<Cluster>& new_cluster)
        {
                clusters_.push_back(std::move(new_cluster));
        }

      private:
        std::vector<std::shared_ptr<Cluster>> clusters_;
        bool freescaling_;
        std::size_t cpt_id_{1};
};

#endif
