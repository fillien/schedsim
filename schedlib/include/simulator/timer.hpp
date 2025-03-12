#ifndef TIMER_HPP
#define TIMER_HPP

#include <simulator/entity.hpp>

#include <functional>
#include <memory>

class Engine;

class Timer : public Entity, public std::enable_shared_from_this<Timer> {
      public:
        Timer(const std::weak_ptr<Engine>& sim, std::function<void()> callback)
            : Entity(sim), callback_(std::move(callback)) {};
        void set(const double& duration);
        void cancel();
        auto is_active() const -> bool { return active_; }
        void fire();
        auto deadline() const -> double { return deadline_; }

      private:
        bool active_{false};
        double deadline_{0};
        std::function<void()> callback_;
};

#endif
