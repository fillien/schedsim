#ifndef TIMER_HPP
#define TIMER_HPP

#include <entity.hpp>
#include <functional>
#include <memory>

class Engine;

class Timer : public Entity, public std::enable_shared_from_this<Timer> {
      public:
        Timer(const std::weak_ptr<Engine>& sim, std::function<void()> callback)
            : Entity(sim), callback(callback) {};
        void set(const double& duration);
        void cancel();
        auto is_active() -> bool { return active_; };
        void fire();
        auto deadline() -> double { return deadline_; };

        std::function<void()> callback;

      private:
        bool active_{false};
        double deadline_{0};
};

#endif
