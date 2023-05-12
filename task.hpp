#ifndef TASK_HPP
#define TASK_HPP

#include "entity.hpp"
#include <memory>

class processor;

class task : public entity {
      public:
        int id;
        double period;
        double utilization;
        double remaining_execution_time{0};

        task(const int id, const double& period, const double& utilization);
        auto is_attached() -> bool;
        auto has_remaining_time() -> bool;

      private:
        std::shared_ptr<processor> attached_proc{nullptr};
};

#endif
