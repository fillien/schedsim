#ifndef SCHED_FFA_DELAY_HPP
#define SCHED_FFA_DELAY_HPP

#include "../engine.hpp"
#include "../entity.hpp"
#include "parallel.hpp"
#include "processor.hpp"
#include "timer.hpp"
#include <cstddef>
#include <memory>

class ffa_delay : public sched_parallel {
      private:
        static constexpr double DELAY_CORE_CHANGE{2};
        static constexpr double DELAY_BEFORE_SLEEP{DELAY_CORE_CHANGE};
        static constexpr double DELAY_FREQUENCY{5};
        std::size_t nb_active_procs{1};
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
        void adjust_active_processors(std::size_t target_processors);

      protected:
        auto get_nb_active_procs(const double& new_utilization) const -> std::size_t override;

      public:
        explicit ffa_delay(const std::weak_ptr<engine> sim);
        void update_platform() override;
};

#endif
