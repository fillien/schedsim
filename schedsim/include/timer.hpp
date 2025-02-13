#ifndef TIMER_HPP
#define TIMER_HPP

#include <entity.hpp>
#include <functional>
#include <memory>

class Engine;

class Timer : public Entity, public std::enable_shared_from_this<Timer> {
      public:
        Timer(const std::weak_ptr<Engine>& sim, std::function<void()> callback)
            : Entity(sim), callback(callback){};
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
