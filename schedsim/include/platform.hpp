#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "entity.hpp"
#include "processor.hpp"

#include <cstddef>
#include <memory>
#include <vector>

/**
 * @brief A platform is a component that contains processors, for example an SoC.
 */
class platform : public entity {
      private:
        std::vector<double> frequencies;
        double effective_freq;
        double current_freq;
        bool freescaling;

        std::shared_ptr<timer> dvfs_timer;
        double dvfs_target;

      public:
        static constexpr double DVFS_DELAY{0.5};

        /**
         * @brief Processors of the platform.
         */
        std::vector<std::shared_ptr<processor>> processors;

        /**
         * @brief A constructor who create the number of processors set in parameters
         * @param nb_proc Number of processors for the platform
         */
        explicit platform(
            const std::weak_ptr<engine>& sim,
            std::size_t nb_proc,
            const std::vector<double>& frequencies,
            const double& effective_freq,
            bool freescaling_allowed);

        [[nodiscard]] auto freq_max() const { return *frequencies.begin(); }
        [[nodiscard]] auto freq_min() const { return *frequencies.rbegin(); }
        [[nodiscard]] auto freq_eff() const { return effective_freq; }
        [[nodiscard]] auto freq() const { return current_freq; };
        [[nodiscard]] auto speed() const { return current_freq / freq_max(); }
        void set_freq(const double& new_freq);

        auto ceil_to_mode(const double& freq) -> double;

        void dvfs_change_freq(const double& next_freq);
};

#endif
