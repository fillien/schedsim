#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <simulator/allocator.hpp>
#include <simulator/event.hpp>
#include <simulator/platform.hpp>

#include <functional>
#include <map>
#include <memory>
#include <protocols/traces.hpp>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template <typename T>
auto from_shared(std::function<bool(const T&)> func)
    -> std::function<bool(const std::shared_ptr<T>&)>
{
        return [func](const std::shared_ptr<T>& arg) -> bool { return func(*arg); };
}

template <typename T>
auto from_shared(std::function<bool(const T&, const T&)> func)
    -> std::function<bool(const std::shared_ptr<T>&, const std::shared_ptr<T>&)>
{
        return [func](const std::shared_ptr<T>& arg1, const std::shared_ptr<T>& arg2) -> bool {
                return func(*arg1, *arg2);
        };
}

/**
 * @brief The Engine class simulates events over time on a given platform
 *        and dispatches them to an attached scheduler.
 */
class Engine {
      public:
        static constexpr double ZERO_ROUNDED = 0.0000001;

        /**
         * @brief Construct an Engine.
         * @param is_there_delay Specifies if delay is activated.
         */
        explicit Engine(bool is_there_delay);

        /**
         * @brief Attach a scheduler to the engine.
         * @param sched Shared pointer to the scheduler.
         */
        auto scheduler(const std::shared_ptr<allocators::Allocator>& alloc) -> void
        {
                alloc_ = alloc;
        }

        /**
         * @brief Attach a platform to the engine.
         * @param platform Shared pointer to the platform.
         */
        auto platform(const std::shared_ptr<Platform>& platform) -> void { platform_ = platform; }

        /**
         * @brief Execute the simulation event loop.
         */
        auto simulation() -> void;

        /**
         * @brief Get the current platform.
         * @return Shared pointer to the platform.
         */
        [[nodiscard]] auto chip() const -> const std::shared_ptr<Platform>& { return platform_; }

        /**
         * @brief Get the current simulation time.
         * @return The current timestamp.
         */
        [[nodiscard]] auto time() const -> double { return current_timestamp_; }

        /**
         * @brief Get the list of future events.
         * @return Const reference to the future events multimap.
         */
        [[nodiscard]] auto future_list() const -> const std::multimap<double, events::Event>&
        {
                return future_list_;
        }

        /**
         * @brief Remove events from the future list that satisfy a predicate.
         * @param pred Predicate function that returns true for events to be removed.
         * @return The number of events removed.
         */
        auto remove_event(const std::function<bool(const std::pair<double, events::Event>&)>& pred)
            -> std::size_t
        {
                return std::erase_if(future_list_, pred);
        }

        /**
         * @brief Schedule a new event.
         * @param new_event The event to be scheduled.
         * @param timestamp The time at which the event should occur.
         */
        auto add_event(const events::Event& new_event, const double& timestamp) -> void;

        /**
         * @brief Log a trace in the past events list.
         * @param new_trace The trace to add.
         */
        auto add_trace(const protocols::traces::trace& new_trace) -> void;

        /**
         * @brief Round a value to zero if it is within a small epsilon range.
         * @param value The value to round.
         * @return The rounded value.
         */
        static auto round_zero(const double& value) -> double
        {
                return (value >= -ZERO_ROUNDED && value <= ZERO_ROUNDED) ? 0.0 : value;
        }

        /**
         * @brief Retrieve the past trace events.
         * @return A multimap of timestamps and traces.
         */
        [[nodiscard]] auto traces() const -> const std::multimap<double, protocols::traces::trace>&
        {
                return past_list_;
        }

        /**
         * @brief Get the attached scheduler.
         * @return Shared pointer to the scheduler.
         */
        [[nodiscard]] auto alloc() const -> const std::shared_ptr<allocators::Allocator>&
        {
                return alloc_;
        }

        /**
         * @brief Check whether delay is activated.
         * @return True if delay is activated, false otherwise.
         */
        [[nodiscard]] auto is_delay_activated() const -> bool { return delay_activated_; }

      private:
        double current_timestamp_{0};                               ///< Simulation time counter.
        std::shared_ptr<allocators::Allocator> alloc_;              ///< Attached scheduler.
        std::shared_ptr<Platform> platform_;                        ///< The simulation platform.
        std::multimap<double, protocols::traces::trace> past_list_; ///< Log of past trace events.
        bool delay_activated_{false}; ///< Flag indicating if delay is activated.
        std::multimap<double, events::Event> future_list_; ///< List of future events.
};

#endif // ENGINE_HPP
