#ifndef SCHED_PARALLEL_HPP
#define SCHED_PARALLEL_HPP

#include "entity.hpp"
#include "processor.hpp"
#include "scheduler.hpp"
#include <iostream>
#include <memory>

class sched_parallel : public scheduler {
        auto get_inactive_bandwidth() -> double;
        auto get_nb_active_procs() -> int;
        static auto processor_order(const std::shared_ptr<processor>& first,
                                    const std::shared_ptr<processor>& second) -> bool;

      public:
        explicit sched_parallel(const std::weak_ptr<engine> sim) : scheduler(sim) {
                std::cout << "setup sched_parallel" << std::endl;
        };

        auto get_server_budget(const std::shared_ptr<server>& serv) -> double;
        auto get_server_new_virtual_time(const std::shared_ptr<server>& serv,
                                         const double& running_time) -> double;
        auto admission_test(const std::shared_ptr<task>& new_task) -> bool;
        void custom_scheduler();
};

#endif
