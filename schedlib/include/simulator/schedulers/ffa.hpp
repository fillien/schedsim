#ifndef SCHED_FFA_HPP
#define SCHED_FFA_HPP

#include <simulator/schedulers/dpm_dvfs.hpp>

namespace scheds {

class Ffa : public DpmDvfs {
      public:
        explicit Ffa(Engine& sim) : DpmDvfs(sim) {}

        void update_platform() override;
};

} // namespace scheds

#endif
