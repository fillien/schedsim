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
         * @brief Return the current active bandwidth
         */
        auto get_active_bandwidth() -> double;

        /**
         * @brief Compute budget of a server
         * @param server The server of which compute the budget
         */
        auto compute_budget(const server& serv) -> double;

        /**
         * @brief Update the virtual time and the remaining execution time of server
         * @param serv The server to update
         * @param time_consumed The duration of the elapsed execution
         */
        void update_server_time(const std::shared_ptr<server>& serv, const double time_comsumed);

        /**
         * @brief Move in the future the deadline of the server
         * @param serv The server to update
         */
        void postpone(const std::shared_ptr<server>& serv);

        /**
         * @brief Change the state of the server to active contending state.
         * @param serv The server
         */
        void goto_active_cont(const std::shared_ptr<server>& serv);

        /**
         * @brief Change the state of the server to active non contending state.
         * @param serv The server
         */
        void goto_active_non_cont(const std::shared_ptr<server>& serv);

        /**
         * @brief Change the state of the server to state idle state.
         * @param serv The server
         */
        void goto_idle(const std::shared_ptr<server>& serv);

        /**
         * @brief A helper function who return a safe pointer to the attached simulation engine.
         */
        auto sim() const -> std::shared_ptr<engine> {
                assert(!simulator.expired());
                return simulator.lock();
        }

        // Mandatory event handler
        void handle_undefined_event(const event& evt);

        // Used event handlers
        void handle_job_arrival(const event& evt);
        void handle_job_finished(const event& evt, bool is_there_new_job);

        void handle_serv_budget_exhausted(const event& evt);
        void handle_sim_finished(const event& evt);
        void handle_serv_idle(const event& evt);

        void resched();

      public:
        /**
         * @brief A setter to attach a simulation engine
         * @param A unsafe pointer to a simulation engine
         */
        void set_engine(std::weak_ptr<engine> simulator);

        void handle(std::vector<event> evts);
};

#endif
