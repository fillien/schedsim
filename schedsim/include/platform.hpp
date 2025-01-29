#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "entity.hpp"
#include "processor.hpp"

#include <cstddef>
#include <memory>
#include <vector>

class scheduler;

class cluster : public entity, public std::enable_shared_from_this<cluster> {
      private:
        std::size_t id;
        std::vector<double> frequencies;
        double effective_freq;
        double current_freq;

        std::shared_ptr<timer> dvfs_timer;
        double dvfs_target;

        std::weak_ptr<scheduler> attached_scheduler;

      public:
        static constexpr double DVFS_DELAY{0.5};

        /**
         * @brief Processors of the platform.
         */
        std::vector<std::shared_ptr<processor>> processors;

        explicit cluster(
            const std::weak_ptr<engine>& sim,
            const std::size_t id,
            const std::vector<double>& frequencies,
            const double& effective_freq);

        [[nodiscard]] auto freq_max() const { return *frequencies.begin(); }
        [[nodiscard]] auto freq_min() const { return *frequencies.rbegin(); }
        [[nodiscard]] auto freq_eff() const { return effective_freq; }
        [[nodiscard]] auto freq() const { return current_freq; };
        [[nodiscard]] auto speed() const { return current_freq / freq_max(); }

        void set_freq(const double& new_freq);
        auto ceil_to_mode(const double& freq) -> double;
        void dvfs_change_freq(const double& next_freq);
        void create_procs(const std::size_t nb_procs);
        auto get_sched() const -> std::weak_ptr<scheduler> { return attached_scheduler; };
        void set_sched(const std::weak_ptr<scheduler>& sched) { attached_scheduler = sched; };
};

/**
 * @brief A platform is a component that contains processors, for example an SoC.
 */
class platform : public entity {
      private:
        bool freescaling;
        std::size_t cpt_id{0};

      public:
        std::vector<std::shared_ptr<cluster>> clusters;

        /**
         * @brief A constructor who create the number of processors set in parameters
         * @param nb_proc Number of processors for the platform
         */
        explicit platform(const std::weak_ptr<engine>& sim, bool freescaling_allowed);
        [[nodiscard]] auto isfreescaling() const -> bool { return freescaling; };
        auto reserve_next_id() -> std::size_t { return cpt_id++; };
};

#endif
