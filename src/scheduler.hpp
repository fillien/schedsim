#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "entity.hpp"
#include "event.hpp"
#include "server.hpp"
#include <memory>
#include <vector>

struct priorities {
        constexpr static int MIN_PRIORITY = 100;
        auto operator()([[maybe_unused]] const events::job_finished& evt) { return 0; };
        auto operator()([[maybe_unused]] const events::serv_budget_exhausted& evt) { return 1; };
        auto operator()([[maybe_unused]] const events::job_arrival& evt) { return 2; };
        auto operator()([[maybe_unused]] const events::serv_inactive& evt) { return 3; };
        auto operator()([[maybe_unused]] auto& evt) { return MIN_PRIORITY; };
};

auto get_priority(const events::event& evt) -> int;

/**
 * @brief A class that handle the events of the system accordingly to a scheduling policy.
 */
class scheduler : public entity {
      private:
        bool need_resched{false};

        void handle_job_arrival(const std::shared_ptr<task>& new_task, const double& job_duration);
        void handle_job_finished(const std::shared_ptr<server>& serv, bool is_there_new_job);
        void handle_serv_budget_exhausted(const std::shared_ptr<server>& serv);
        void handle_serv_inactive(const std::shared_ptr<server>& serv);
        void resched();
        void update_server_times(const std::shared_ptr<server>& serv);

      protected:
        /**
         * @brief A vector to track and own servers objects
         */
        std::vector<std::shared_ptr<server>> servers;

        static auto is_running_server(const std::shared_ptr<server>& current_server) -> bool;
        static auto is_ready_server(const std::shared_ptr<server>& current_server) -> bool;
        static auto is_active_server(const std::shared_ptr<server>& current_server) -> bool;
        static auto deadline_order(const std::shared_ptr<server>& first,
                                   const std::shared_ptr<server>& second) -> bool;

        void resched_proc(const std::shared_ptr<processor>& proc_with_server,
                          const std::shared_ptr<server>& server_to_execute);

        virtual auto get_server_new_virtual_time(const std::shared_ptr<server>& serv,
                                                 const double& running_time) -> double = 0;
        virtual auto get_server_budget(const std::shared_ptr<server>& serv) -> double = 0;
        virtual auto admission_test(const std::shared_ptr<task>& new_task) -> bool = 0;
        virtual void custom_scheduler() = 0;

      public:
        explicit scheduler(const std::weak_ptr<engine> sim) : entity(sim){};
        virtual ~scheduler() = default;

        void handle(std::vector<events::event> evts);
        [[nodiscard]] auto get_active_bandwidth() const -> double;
        auto make_server(const std::shared_ptr<task>& new_task) -> std::shared_ptr<server>;
};

#endif
