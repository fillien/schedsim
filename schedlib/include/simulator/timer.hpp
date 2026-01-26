#ifndef TIMER_HPP
#define TIMER_HPP

#include <simulator/entity.hpp>

#include <functional>

class Engine;

/**
 * @brief Represents a timer that can be set to execute a callback function after a specified
 * duration.
 */
class Timer : public Entity {
      public:
        /**
         * @brief Constructs a new Timer object.
         *
         * @param sim A reference to the Engine that owns this timer.
         * @param callback The function to be called when the timer expires.
         */
        Timer(Engine& sim, std::function<void()> callback)
            : Entity(sim), callback_(std::move(callback)) {};

        /**
         * @brief Sets the timer for a specified duration.
         *
         * @param duration The duration in seconds until the timer should expire.
         */
        void set(const double& duration);

        /**
         * @brief Cancels the timer.
         */
        void cancel();

        /**
         * @brief Checks if the timer is currently active.
         *
         * @return `true` if the timer is active; otherwise, `false`.
         */
        auto is_active() const -> bool { return active_; };

        /**
         * @brief Fires the timer's callback function.
         */
        void fire();

        /**
         * @brief Returns the timer's deadline.
         *
         * @return The absolute time at which the timer will expire.
         */
        auto deadline() const -> double { return deadline_; };

      private:
        bool active_{false};
        double deadline_{0};
        std::function<void()> callback_;
};

#endif
