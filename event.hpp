#ifndef EVENT_HPP
#define EVENT_HPP

#include "entity.hpp"

#include <memory>
#include <ostream>
#include <vector>

/// Possible kinds of events
enum class types {
        JOB_ARRIVAL,
        JOB_FINISHED,
        PROC_ACTIVATED,
        PROC_IDLED,
        RESCHED,
        SERV_ACT_CONT,
        SERV_ACT_NON_CONT,
        SERV_BUDGET_EXHAUSTED,
        SERV_BUDGET_REPLENISHED,
        SERV_IDLE,
        SERV_RUNNING,
        SIM_FINISHED,
        TASK_PREEMPTED,
        TASK_SCHEDULED,
};

class event {
      public:
        int id;
        types type;
        const std::weak_ptr<entity> target;
        double payload;
        int priority;

        event(const types type, const std::weak_ptr<entity> target, const double payload,
              const int priority = 0);

      private:
        static inline int cpt_id{0};
};

auto operator<<(std::ostream& out, const types& type) -> std::ostream&;
auto operator<<(std::ostream& out, const event& ev) -> std::ostream&;

#endif
