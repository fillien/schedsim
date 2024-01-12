#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "entity.hpp"
#include "processor.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

/**
 * @brief A platform is a component that contains processors, for example an SoC.
 */
class platform : public entity {
      private:
        std::vector<double> frequencies;
        double current_freq;
        bool freescaling;

      public:
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
            bool freescaling_allowed);

        [[nodiscard]] auto get_f_max() const { return *frequencies.begin(); }
        [[nodiscard]] auto get_freq() const { return current_freq; };
        void set_freq(const double& new_freq);
};

#endif
