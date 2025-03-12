#ifndef SCHED_CSF_HPP
#define SCHED_CSF_HPP

#include <schedulers/dpm_dvfs.hpp>

namespace scheds {
class Csf : public DpmDvfs {
      public:
        explicit Csf(const std::weak_ptr<Engine>& sim) : DpmDvfs(sim) {}
        void update_platform() override;
};
} // namespace scheds

#endif
