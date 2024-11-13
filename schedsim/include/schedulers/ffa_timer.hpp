#ifndef SCHED_FFA_TIMER_HPP
#define SCHED_FFA_TIMER_HPP

#include "../engine.hpp"
#include "../entity.hpp"
#include "parallel.hpp"
#include "processor.hpp"
#include "timer.hpp"
#include <cstddef>
#include <memory>

class ffa_timer : public sched_parallel {
      private:
        static constexpr double DELAY_CORE_CHANGE{2};
        static constexpr double DELAY_BEFORE_SLEEP{DELAY_CORE_CHANGE};
        std::size_t nb_active_procs{1};
        std::vector<std::shared_ptr<timer>> core_timers;
        std::shared_ptr<timer> freq_timer;

        auto compute_freq_min(
            const double& freq_max,
            const double& total_util,
            const double& max_util,
            const double& nb_procs) -> double;
        auto cores_on_timer() -> std::size_t;
        auto cores_on_sleep() -> std::size_t;
        void cancel_next_timer();
        void activate_next_core();
        void put_next_core_to_bed();
        void change_state_proc(
            const processor::state next_state, const std::shared_ptr<processor>& proc);

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit ffa_timer(const std::weak_ptr<engine> sim) : sched_parallel(sim)
        {
                core_timers.resize(sim.lock()->chip()->processors.size());
                for (std::size_t i = 0; i < sim.lock()->chip()->processors.size(); i++) {
                        core_timers.push_back(std::make_shared<timer>(sim, [&]() {
                                std::vector<std::shared_ptr<processor>> copy_chip =
                                    sim.lock()->chip()->processors;
                                std::sort(
                                    copy_chip.begin(),
                                    copy_chip.end(),
                                    from_shared<processor>(processor_order));
                                auto it = copy_chip.cbegin();
                                bool found_idle_proc{false};
                                while (!found_idle_proc) {
                                        if ((*it)->get_state() != processor::state::sleep) {
                                                change_state_proc(processor::state::sleep, (*it));
                                                found_idle_proc = true;
                                        }
                                }
                        }));
                }
        };

        void update_platform() override;
};

#endif
