#include "plateform.hpp"
#include "processor.hpp"

#include <cstddef>
#include <vector>

plateform::plateform(){};

plateform::plateform(const size_t nb_proc) {
        for (size_t i = 0; i < nb_proc; ++i) {
                auto new_proc = std::make_shared<processor>(i);
                processors.push_back(std::move(new_proc));
        }
}
