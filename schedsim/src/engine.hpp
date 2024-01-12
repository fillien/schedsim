#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "event.hpp"
#include "platform.hpp"
#include "scheduler.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <protocols/traces.hpp>
#include <sstream>
#include <variant>

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
class engine {
      private:
        /**
         * @brief A counter of the time passing.
         */
        double current_timestamp{0};

        /**
         * @brief The attached scheduler.
         */
        std::shared_ptr<scheduler> sched;

        /**
         * @brief A model of the platform on which the scheduler will operate.
         * @TODO Maybe refactor this as a unique_ptr, as it will be alone all the running time
         */
        std::shared_ptr<platform> current_platform;

        /**
         * @brief The list of past events, a pair of the timestamp of the event and the event
         * itself.
         */
        std::multimap<double, protocols::traces::trace> past_list{};

      public:
        static constexpr double ZERO_ROUNDED = 0.0000001;

        /**
         * @brief The list of future events, a pair of the timestamp of the event and the event
         * itself.
         */
        std::multimap<double, events::event> future_list{};

        /**
         * @brief A constructor to help generate the platform.
         */
        explicit engine() = default;

        /**
         * @brief Setter to attach a scheduler.
         * @param new_sched The new scheduler to attach.
         */
        void set_scheduler(const std::shared_ptr<scheduler>& new_sched) { sched = new_sched; }

        /**
         * @brief Setter to attach a platform.
         * @param new_platform The new platform to attach.
         */
        void set_platform(const std::shared_ptr<platform>& new_platform)
        {
                current_platform = new_platform;
        };

        /**
         * @brief The main function of the simulator engine, the event loop that handles the events.
         */
        void simulation();

        /**
         * @return The current platform attached to the engine.
         */
        [[nodiscard]] auto get_platform() const -> std::shared_ptr<platform>
        {
                return current_platform;
        };

        /**
         * @return The current simulation time.
         */
        [[nodiscard]] auto get_time() const -> double { return current_timestamp; };

        /**
         * @return The future list of events.
         */
        [[nodiscard]] auto get_future_list() const -> std::multimap<double, events::event>
        {
                return this->future_list;
        };

        /**
         * @brief Add a new event to the future list.
         * @param new_event The new event to add.
         * @param timestamp The timestamp at which the event should occur.
         */
        void add_event(const events::event& new_event, const double& timestamp);

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
                if (value >= -ZERO_ROUNDED || value <= ZERO_ROUNDED) { return 0; }
                return value;
        }

        /**
         * @return The traces (past events) recorded by the engine.
         */
        auto get_traces() { return past_list; };
};

#endif
