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
        SERV_POSTPONE,
        SIM_FINISHED,
        TASK_PREEMPTED,
        TASK_SCHEDULED,
        VIRTUAL_TIME_UPDATE
};

class event {
      public:
        int id;
        types type;
        std::weak_ptr<entity> target;
        double payload;

        event(const types type, const std::weak_ptr<entity> target, const double payload);

      private:
        static inline int cpt_id{0};
};

struct trace {
        double timestamp;
        types type;
        int target_id;
        double payload;
};

auto operator<<(std::ostream& out, const types& type) -> std::ostream&;
auto operator<<(std::ostream& out, const event& ev) -> std::ostream&;

#endif
