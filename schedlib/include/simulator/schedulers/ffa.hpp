#ifndef SCHED_FFA_HPP
#define SCHED_FFA_HPP

#include <schedulers/dpm_dvfs.hpp>

namespace scheds {
class Ffa : public DpmDvfs {
      public:
        explicit Ffa(const std::weak_ptr<Engine>& sim) : DpmDvfs(sim) {}
        void update_platform() override;
};
} // namespace scheds

#endif
