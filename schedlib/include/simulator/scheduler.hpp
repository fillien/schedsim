#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <simulator/entity.hpp>
#include <simulator/event.hpp>
#include <simulator/platform.hpp>
#include <simulator/processor.hpp>
#include <simulator/server.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace scheds {

/**
 * @brief Represents a scheduler responsible for managing tasks and servers within a simulation
 * environment.
 */
class Scheduler : public Entity {
      public:
        explicit Scheduler(Engine& sim) : Entity(sim) {}
        virtual ~Scheduler() = default;
        Scheduler(const Scheduler&) = delete;
        auto operator=(const Scheduler&) -> Scheduler& = delete;
        Scheduler(Scheduler&&) noexcept = delete;
        auto operator=(Scheduler&&) noexcept -> Scheduler& = delete;

        virtual auto admission_test(const Task& new_task) const -> bool = 0;

        auto handle(const events::Event& evt) -> void;
        auto call_resched() -> void;
        auto is_this_my_event(const events::Event& evt) -> bool;

        auto cluster(Cluster* clu) -> void { attached_cluster_ = clu; }
        [[nodiscard]] auto cluster() const -> Cluster* { return attached_cluster_; }

        auto u_max() const -> double;
        [[nodiscard]] auto active_bandwidth() const -> double;

        auto on_job_arrival(Task* new_task, const double& job_duration) -> void;

        auto servers() const -> const std::vector<std::unique_ptr<Server>>& { return servers_; }

        auto total_utilization() const -> double;

        auto last_utilizations() const -> std::vector<std::pair<double, double>>
        {
                return last_utilizations_;
        };

        void set_resched_callback(std::function<void(Scheduler*)> cb)
        {
                resched_callback_ = std::move(cb);
        }

        void request_resched();

      protected:
        [[nodiscard]] auto chip() const -> Cluster*;

        static auto has_job_server(const Server& serv) -> bool;
        static auto deadline_order(const Server& first, const Server& second) -> bool;

        auto resched_proc(Processor* proc_with_server, Server* server_to_execute) -> void;
        auto update_server_times(Server* serv) -> void;
        auto update_running_servers() -> void;
        auto cancel_alarms(const Server& serv) -> void;
        auto activate_alarms(Server* serv) -> void;
        auto clamp(const double& nb_procs) -> double;

        virtual auto server_virtual_time(const Server& serv, const double& running_time)
            -> double = 0;
        virtual auto server_budget(const Server& serv) const -> double = 0;
        virtual auto on_resched() -> void = 0;
        virtual auto on_active_utilization_updated() -> void = 0;
        virtual auto update_platform() -> void = 0;

      private:
        auto on_job_finished(Server* serv, bool is_there_new_job) -> void;
        auto on_serv_budget_exhausted(Server* serv) -> void;
        auto on_serv_inactive(Server* serv) -> void;
        auto detach_server_if_needed(Task* inactive_task) -> void;

        std::vector<std::unique_ptr<Server>> servers_;
        Cluster* attached_cluster_{nullptr};

        double total_utilization_{0.0};

        std::vector<std::pair<double, double>> last_utilizations_;
        std::function<void(Scheduler*)> resched_callback_;
};

} // namespace scheds

#endif // SCHEDULER_HPP
