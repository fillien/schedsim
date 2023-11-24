#include "plateform.hpp"
#include "processor.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <vector>

plateform::plateform(const std::weak_ptr<engine>& sim, size_t nb_proc) : entity(sim)
{
        assert(nb_proc > 0);
        for (size_t i = 1; i <= nb_proc; ++i) {
                auto new_proc = std::make_shared<processor>(sim, i);
                processors.push_back(std::move(new_proc));
        }
}
