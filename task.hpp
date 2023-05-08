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

        task(const int id, const double& period, const double& utilization);
        void set_remaining_execution_time(const double& new_remaining_execution_time);
        auto is_attached() -> bool;
        auto has_remaining_time() -> bool;
        inline auto get_remaining_time() -> double { return remaining_execution_time; }

      private:
        double remaining_execution_time{0};
        std::shared_ptr<processor> attached_proc{nullptr};
};

#endif
