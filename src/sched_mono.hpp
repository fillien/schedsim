#ifndef SCHED_MONO_HPP
#define SCHED_MONO_HPP

#include "entity.hpp"
#include "processor.hpp"
#include "scheduler.hpp"
#include "server.hpp"
#include <iostream>
#include <memory>

class sched_mono : public scheduler {
        std::shared_ptr<processor> proc;

        auto get_active_bandwidth() -> double;
        static auto is_in_runqueue(const std::shared_ptr<server>& current_server) -> bool;

      public:
        explicit sched_mono(
            const std::weak_ptr<engine> sim, const std::shared_ptr<processor>& attached_proc)
            : scheduler(sim), proc(std::move(attached_proc))
        {
        }

        auto get_server_budget(const std::shared_ptr<server>& serv) -> double override;
        auto
        get_server_new_virtual_time(const std::shared_ptr<server>& serv, const double& running_time)
            -> double override;
        auto admission_test(const std::shared_ptr<task>& new_task) -> bool override;
        void custom_scheduler() override;
};

#endif
