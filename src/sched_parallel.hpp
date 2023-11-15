#ifndef SCHED_PARALLEL_HPP
#define SCHED_PARALLEL_HPP

#include "entity.hpp"
#include "processor.hpp"
#include "scheduler.hpp"
#include <iostream>
#include <memory>

class sched_parallel : public scheduler {
        auto get_inactive_bandwidth() const -> double;
        auto get_nb_active_procs(const double& new_utilization = 0) const -> std::size_t;
        static auto processor_order(const processor& first, const processor& second) -> bool;

      public:
        explicit sched_parallel(const std::weak_ptr<engine> sim) : scheduler(sim){};
        auto get_server_budget(const server& serv) const -> double override;
        auto get_server_virtual_time(const server& serv, const double& running_time)
            -> double override;
        auto admission_test(const task& new_task) const -> bool override;
        void custom_scheduler() override;
};

#endif
