#include "processor.hpp"

processor::processor(const std::weak_ptr<engine> sim, const int id) : entity(sim), id(id){};
