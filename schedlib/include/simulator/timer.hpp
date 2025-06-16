#ifndef TIMER_HPP
#define TIMER_HPP

#include <simulator/entity.hpp>

#include <functional>
#include <memory>

class Engine;

/**
 * @brief Represents a timer that can be set to execute a callback function after a specified
 * duration.
 *
 * This class inherits from `Entity` and provides functionality for setting, canceling,
 * and checking the status of a timer.  It uses a callback function to perform an action
 * when the timer expires.
 */
class Timer : public Entity, public std::enable_shared_from_this<Timer> {
      public:
        /**
         * @brief Constructs a new Timer object.
         *
         * @param sim A weak pointer to the Engine that owns this timer.
         * @param callback The function to be called when the timer expires.  This is moved into the
         * Timer object.
         */
        Timer(const std::weak_ptr<Engine>& sim, std::function<void()> callback)
            : Entity(sim), callback_(std::move(callback)) {};

        /**
         * @brief Sets the timer for a specified duration.
         *
         * This function activates the timer and sets its deadline to the current time plus the
         * given duration. If the timer is already active, it resets the existing deadline.
         *
         * @param duration The duration in seconds until the timer should expire.
         */
        void set(const double& duration);

        /**
         * @brief Cancels the timer.
         *
         * This function deactivates the timer, preventing it from firing.
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
         *
         * This function executes the callback associated with the timer and deactivates it.  It
         * should be called by the Engine when the deadline is reached.
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
