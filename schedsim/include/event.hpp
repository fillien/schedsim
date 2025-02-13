#ifndef EVENT_HPP
#define EVENT_HPP

#include <memory>
#include <server.hpp>
#include <task.hpp>
#include <timer.hpp>
#include <variant>

namespace events {

/**
 * @brief Represents an event for the arrival of a job.
 */
struct job_arrival {
        std::shared_ptr<Task> task_of_job; /**< The task associated with the arrived job. */
        double job_duration;               /**< The duration of the arrived job. */
};

/**
 * @brief Represents an event for the completion of a job on a server.
 */
struct job_finished {
        std::shared_ptr<Server> server_of_job; /**< The server where the job is completed. */
        bool is_there_new_job =
            false; /**< Give info about a job_finished event at the same timestep. */
};

/**
 * @brief Represents an event for the exhaustion of the budget on a server.
 */
struct serv_budget_exhausted {
        std::shared_ptr<Server> serv; /**< The server with an exhausted budget. */
};

/**
 * @brief Represents an event for the inactivity of a server.
 */
struct serv_inactive {
        std::shared_ptr<Server> serv; /**< The inactive server. */
};

struct timer_isr {
        std::shared_ptr<timer> target_timer;
};

/**
 * @brief A variant type representing different types of events.
 */
using event =
    std::variant<job_arrival, job_finished, serv_budget_exhausted, serv_inactive, timer_isr>;

} // namespace events

#endif // EVENT_HPP
