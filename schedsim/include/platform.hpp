#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include <cstddef>
#include <entity.hpp>
#include <memory>
#include <processor.hpp>
#include <vector>

namespace scheds {
class scheduler;
}

class Cluster : public entity, public std::enable_shared_from_this<Cluster> {
      private:
        std::size_t id;
        std::vector<double> frequencies;
        double effective_freq;
        double current_freq;
        double perf_score;

        std::shared_ptr<timer> dvfs_timer;
        double dvfs_target;

        std::weak_ptr<scheds::scheduler> attached_scheduler;

      public:
        static constexpr double DVFS_DELAY{0.5};

        /**
         * @brief Processors of the platform.
         */
        std::vector<std::shared_ptr<processor>> processors;

        explicit Cluster(
            const std::weak_ptr<engine>& sim,
            const std::size_t id,
            const std::vector<double>& frequencies,
            const double& effective_freq,
            const double& perf_score);

        [[nodiscard]] auto freq_max() const { return *frequencies.begin(); }
        [[nodiscard]] auto freq_min() const { return *frequencies.rbegin(); }
        [[nodiscard]] auto freq_eff() const { return effective_freq; }
        [[nodiscard]] auto freq() const { return current_freq; };
        [[nodiscard]] auto speed() const { return current_freq / freq_max(); }
        [[nodiscard]] auto perf() const { return perf_score; }
        [[nodiscard]] auto get_id() const { return id; };

        void set_freq(const double& new_freq);
        auto ceil_to_mode(const double& freq) -> double;
        void dvfs_change_freq(const double& next_freq);
        void create_procs(const std::size_t nb_procs);
        auto get_sched() const -> std::weak_ptr<scheds::scheduler> { return attached_scheduler; };
        void set_sched(const std::weak_ptr<scheds::scheduler>& sched)
        {
                attached_scheduler = sched;
        };
};

/**
 * @brief A platform is a component that contains processors, for example an SoC.
 */
class Platform : public entity {
      private:
        bool freescaling;
        std::size_t cpt_id{0};

      public:
        std::vector<std::shared_ptr<Cluster>> clusters;

        /**
         * @brief A constructor who create the number of processors set in parameters
         * @param nb_proc Number of processors for the platform
         */
        explicit Platform(const std::weak_ptr<engine>& sim, bool freescaling_allowed);
        [[nodiscard]] auto isfreescaling() const -> bool { return freescaling; };
        auto reserve_next_id() -> std::size_t { return cpt_id++; };
};

#endif
