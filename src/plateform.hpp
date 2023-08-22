#ifndef PLATEFORM_HPP
#define PLATEFORM_HPP

#include "entity.hpp"
#include "processor.hpp"

#include <memory>
#include <vector>

/**
 * @brief A platform is a component that contains processors, for example an SoC.
 */
class plateform : public entity {
      public:
        /**
         * @brief Processors of the plateform.
         */
        std::vector<std::shared_ptr<processor>> processors;

        /**
         * @brief A constructor who create the number of processors set in parameters
         * @param nb_proc Number of processors for the plateform
         */
        explicit plateform(const std::weak_ptr<engine> sim, const std::size_t nb_proc);
};

#endif
