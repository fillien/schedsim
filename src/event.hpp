#ifndef EVENT_HPP
#define EVENT_HPP

#include "server.hpp"
#include "task.hpp"

#include <memory>
#include <variant>

namespace events {

/**
 * @brief Represents an event for the arrival of a job.
 */
struct job_arrival {
        std::shared_ptr<task> task_of_job; /**< The task associated with the arrived job. */
        double job_duration;               /**< The duration of the arrived job. */
};

/**
 * @brief Represents an event for the completion of a job on a server.
 */
struct job_finished {
        std::shared_ptr<server> server_of_job; /**< The server where the job is completed. */
};

/**
 * @brief Represents an event for the exhaustion of the budget on a server.
 */
struct serv_budget_exhausted {
        std::shared_ptr<server> serv; /**< The server with an exhausted budget. */
};

/**
 * @brief Represents an event for the inactivity of a server.
 */
struct serv_inactive {
        std::shared_ptr<server> serv; /**< The inactive server. */
};

/**
 * @brief A variant type representing different types of events.
 */
using event = std::variant<job_arrival, job_finished, serv_budget_exhausted, serv_inactive>;

} // namespace events

#endif // EVENT_HPP
