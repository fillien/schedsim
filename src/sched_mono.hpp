#ifndef SCHED_MONO_HPP
#define SCHED_MONO_HPP

#include "entity.hpp"
#include "scheduler.hpp"

class sched_mono : public scheduler {
      public:
        explicit sched_mono(std::weak_ptr<engine> sim) : scheduler(sim) {}
};

#endif
