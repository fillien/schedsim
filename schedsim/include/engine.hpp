#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <allocator.hpp>
#include <event.hpp>
#include <functional>
#include <map>
#include <memory>
#include <platform.hpp>
#include <protocols/traces.hpp>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

/// Define utility functions to convert functions operating on raw pointers to functions on shared
/// pointers
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
 * @brief According to a platform and a scheduler, the class simulates, in order of time,
 * each event contained in the future list.
 */
class Engine {
      private:
        /**
         * @brief A counter of the time passing.
         */
        double current_timestamp_{0};

        /**
         * @brief The attached scheduler.
         */
        std::shared_ptr<allocators::Allocator> sched_;

        /**
         * @brief A model of the platform on which the scheduler will operate.
         * @TODO Maybe refactor this as a unique_ptr, as it will be alone all the running time
         */
        std::shared_ptr<Platform> platform_;

        /**
         * @brief The list of past events, a pair of the timestamp of the event and the event
         * itself.
         */
        std::multimap<double, protocols::traces::trace> past_list_{};

        bool delay_activated_{false};

        /**
         * @brief The list of future events, a pair of the timestamp of the event and the event
         * itself.
         */
        std::multimap<double, events::Event> future_list_{};

      public:
        static constexpr double ZERO_ROUNDED = 0.0000001;

        /**
         * @brief A constructor to help generate the platform.
         */
        Engine(const bool is_there_delay);

        /**
         * @brief Setter to attach a scheduler.
         * @param new_sched The new scheduler to attach.
         */
        void scheduler(const std::shared_ptr<allocators::Allocator>& sched) { sched_ = sched; }

        /**
         * @brief Setter to attach a platform.
         * @param new_platform The new platform to attach.
         */
        void platform(const std::shared_ptr<Platform>& platform) { platform_ = platform; };

        /**
         * @brief The main function of the simulator engine, the event loop that handles the events.
         */
        void simulation();

        /**
         * @return The current platform attached to the engine.
         */
        [[nodiscard]] auto chip() const -> std::shared_ptr<Platform> { return platform_; };

        /**
         * @return The current simulation time.
         */
        [[nodiscard]] auto time() const -> double { return current_timestamp_; };

        /**
         * @return The future list of events.
         */
        [[nodiscard]] auto future_list() const -> std::multimap<double, events::Event>
        {
                return future_list_;
        };

        auto remove_event(std::function<bool(const std::pair<double, events::Event>&)> pred)
            -> std::size_t
        {
                return std::erase_if(future_list_, pred);
        };

        /**
         * @brief Add a new event to the future list.
         * @param new_event The new event to add.
         * @param timestamp The timestamp at which the event should occur.
         */
        void add_event(const events::Event& new_event, const double& timestamp);

        /**
         * @brief Add a trace to the logs (past list).
         * @param new_trace The trace to add.
         */
        void add_trace(const protocols::traces::trace& new_trace);

        /**
         * @brief Round a double value to zero if it is within a small range.
         * @param value The value to round.
         * @return The rounded value.
         */
        static auto round_zero(const double& value) -> double
        {
                if (value >= -ZERO_ROUNDED && value <= ZERO_ROUNDED) { return 0; }
                return value;
        }

        /**
         * @return The traces (past events) recorded by the engine.
         */
        auto traces() const { return past_list_; };

        auto sched() -> std::shared_ptr<allocators::Allocator> { return sched_; };

        [[nodiscard]] auto is_delay_activated() const -> bool { return delay_activated_; };
};

#endif
