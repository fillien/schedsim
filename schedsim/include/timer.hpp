#ifndef TIMER_HPP
#define TIMER_HPP

#include "entity.hpp"
#include <functional>
#include <memory>

class engine;

class timer : public entity, public std::enable_shared_from_this<timer> {
      public:
        timer(const std::weak_ptr<engine>& sim, std::function<void()> callback)
            : entity(sim), callback(callback){};
        void set(const double& duration);
        void cancel();
        auto is_active() -> bool { return active; };
        void fire();
        auto get_deadline() -> double { return deadline; };

        std::function<void()> callback;

      private:
        bool active{false};
        double deadline{0};
};

#endif
