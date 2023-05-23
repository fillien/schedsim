#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "event.hpp"
#include "plateform.hpp"
#include "scheduler.hpp"
#include "tracer.hpp"

#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>

class engine : public std::enable_shared_from_this<engine> {
        void handle(const event& evt);

      public:
        std::shared_ptr<scheduler> sched;
        plateform current_plateform;
        tracer logging_system;

        std::multimap<double, const event> future_list{};
        double current_timestamp{0};

        explicit engine(const size_t nb_processors);
        void set_scheduler(std::shared_ptr<scheduler>& new_sched);
        auto get_shared() -> std::shared_ptr<engine> { return shared_from_this(); };

        void simulation();
        void add_event(const event& new_evt, const double timestamp);
};

#endif
