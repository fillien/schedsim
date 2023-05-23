#ifndef PLATEFORM_HPP
#define PLATEFORM_HPP

#include "entity.hpp"
#include "processor.hpp"

#include <memory>
#include <vector>

/// A platform is a component that contains processors, for example an SoC
class plateform : public entity {
      public:
        /// Processors of the plateform
        std::vector<std::shared_ptr<processor>> processors;

        plateform();

        /** A constructor who create the number of processors set in parameters
         *
         * @param nb_proc Number of processors for the plateform
         */
        plateform(const std::size_t nb_proc);
};

#endif
