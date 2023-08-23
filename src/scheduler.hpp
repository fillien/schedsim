#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
#include "server.hpp"
#include "tracer.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <vector>

class engine;

/**
 * @brief A class that handle the events of the system accordingly to a scheduling policy.
 */
class scheduler {
        /**
         * @brief The pointer to access the simulation engine
         */
        std::weak_ptr<engine> simulator;

        /**
         * @brief A vector to track and own servers objects
         */
        std::vector<std::shared_ptr<server>> servers;

        /**
         * @brief Store the last timestamp at which the runqueue has been rescheduled
         */
        double last_resched{0};

        bool need_resched{false};

        /**
         * @brief Helper function to add a new trace to the logs.
         * @param type The kind of event
         * @param target_id Id of the object that is link to the kind of event
         * @param payload A optionnal payload
         */
        void add_trace(const types type, const int target_id, const double payload = 0) const;

        /**
         * @brief Check if a type of event is present at the current timestamp and act on a task
         * @param the_task The task on which the event is applied
         * @param type The type of event to looking for
         */
        auto is_event_present(const std::shared_ptr<task>& the_task, const types type) -> bool;

        /**
         * @brief A helper function who return a safe pointer to the attached simulation engine.
         */
        auto sim() const -> std::shared_ptr<engine> {
                assert(!simulator.expired());
                return simulator.lock();
        }

        void handle_job_arrival(const std::shared_ptr<task>& new_task, const double& job_wcet);
        void handle_job_finished(const std::shared_ptr<server>& serv, const double& deltatime,
                                 bool is_there_new_job);
        void handle_serv_budget_exhausted(const std::shared_ptr<server>& serv,
                                          const double& deltatime);
        void handle_serv_inactive(const std::shared_ptr<server>& serv, const double& deltatime);
        void resched();

      public:
        /**
         * @brief A setter to attach a simulation engine
         * @param A unsafe pointer to a simulation engine
         */
        void set_engine(std::weak_ptr<engine> new_sim) {
                assert(!new_sim.expired());
                simulator = new_sim;
        }

        void handle(std::vector<event> evts, const double& deltatime);

        /**
         * @brief Return the current active bandwidth
         */
        auto get_active_bandwidth() -> double;
};

#endif
