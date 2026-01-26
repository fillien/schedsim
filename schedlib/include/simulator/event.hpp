#ifndef EVENT_HPP
#define EVENT_HPP

#include <variant>

class Task;
class Server;
class Timer;

namespace events {

/**
 * @brief Represents an event for the arrival of a job.
 */
struct JobArrival {
        /**
         * @brief The task associated with the arrived job.
         */
        Task* task_of_job;
        /**
         * @brief The duration of the arrived job.
         */
        double job_duration;
};

/**
 * @brief Represents an event for the completion of a job on a server.
 */
struct JobFinished {
        /**
         * @brief The server where the job is completed.
         */
        Server* server_of_job;
        /**
         * @brief Give info about a JobFinished event at the same timestep.
         */
        bool is_there_new_job = false;
};

/**
 * @brief Represents an event for the exhaustion of the budget on a server.
 */
struct ServBudgetExhausted {
        /**
         * @brief The server with an exhausted budget.
         */
        Server* serv;
};

/**
 * @brief Represents an event for the inactivity of a server.
 */
struct ServInactive {
        /**
         * @brief The inactive server.
         */
        Server* serv;
};

struct TimerIsr {
        /**
         * @brief The target timer.
         */
        Timer* target_timer;
};

/**
 * @brief A variant type representing different types of events.
 */
using Event = std::variant<JobArrival, JobFinished, ServBudgetExhausted, ServInactive, TimerIsr>;

} // namespace events

#endif // EVENT_HPP
