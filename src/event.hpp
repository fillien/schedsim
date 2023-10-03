#ifndef EVENT_HPP
#define EVENT_HPP

#include "entity.hpp"

#include <memory>
#include <ostream>
#include <vector>

/**
 * @brief Possible kinds of events.
 */
enum class types {
        JOB_ARRIVAL,
        JOB_FINISHED,
        PROC_ACTIVATED,
        PROC_IDLED,
        RESCHED,
        SERV_BUDGET_EXHAUSTED,
        SERV_BUDGET_REPLENISHED,
        SERV_INACTIVE,
        SERV_NON_CONT,
        SERV_POSTPONE,
        SERV_READY,
        SERV_RUNNING,
        SIM_FINISHED,
        TASK_PREEMPTED,
        TASK_SCHEDULED,
        TASK_REJECTED,
        VIRTUAL_TIME_UPDATE
};

/**
 * @brief An event qualifed by a type, the object who is concerned by the event and a optionnal
 * payload.
 */
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

/**
 * @brief A trace, that like an event but it accept a timestamp and its stored in logs to be
 * printed.
 */
struct trace {
        /**
         * @brief The time at which the trace as been registered.
         */
        double timestamp;

        /**
         * @brief The kind of event that as been executed.
         */
        types type;

        /**
         * @brief The id of the entity of the event.
         */
        int target_id;

        /**
         * @brief The optionnal payload of the event.
         */
        double payload;
};

/**
 * @brief Serialize a type object to a output stream.
 * @param out The ouput stream.
 * @param type The type object.
 */
auto operator<<(std::ostream& out, const types& type) -> std::ostream&;

/**
 * @brief Serialize a event object to a output stream.
 * @param out The ouput stream.
 * @param type The event object.
 */
auto operator<<(std::ostream& out, const event& ev) -> std::ostream&;

#endif
