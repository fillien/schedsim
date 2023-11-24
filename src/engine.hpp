#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "event.hpp"
#include "plateform.hpp"
#include "scheduler.hpp"
#include "traces.hpp"

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <variant>

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
 * @brief According to a platform and a scheduler, the class simulate in order of time each events
 * contains in the future list
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
         * @brief A model a the platform on which the scheduler will operate.
         * @TODO Maybe refactor this in a unique_ptr, I mean it'll be alone all the running time
         */
        std::shared_ptr<plateform> current_plateform;

        /**
         * @brief The list of past events, it's a pair of the timestamp of the event and the event
         * to process himself.
         */
        std::multimap<double, traces::trace> past_list{};

      public:
        static constexpr double ZERO_ROUNDED = 0.0000001;

        /**
         * @brief The list of future events, it's a pair of the timestamp of the event and the event
         * to process himself.
         */
        std::multimap<double, events::event> future_list{};

        /**
         * @brief A constructor to help generate the platform.
         */
        explicit engine() = default;

        /**
         * @brief Setter to attach a scheduler.
         * @param new_sched
         */
        void set_scheduler(const std::shared_ptr<scheduler>& new_sched) { sched = new_sched; }

        /**
         * @brief Setter to attach a plateform.
         * @param new_plateform
         */
        void set_plateform(const std::shared_ptr<plateform>& new_plateform)
        {
                current_plateform = new_plateform;
        };

        /**
         * @brief That the main function of the simulator engine, its the event loop that handle the
         * events.
         */
        void simulation();

        [[nodiscard]] auto get_plateform() const -> std::shared_ptr<plateform>
        {
                return current_plateform;
        };

        [[nodiscard]] auto get_time() const -> double { return current_timestamp; };

        [[nodiscard]] auto get_future_list() const -> std::multimap<double, events::event>
        {
                return this->future_list;
        };

        /**
         * @brief Add new event to the the future list.
         */
        void add_event(const events::event& new_event, const double& timestamp);

        /**
         * @brief Add a trace to logs (past list)
         */
        void add_trace(const traces::trace& new_trace);

        static auto round_zero(const double& value) -> double
        {
                if (value >= -ZERO_ROUNDED || value <= ZERO_ROUNDED) { return 0; }
                return value;
        }

        auto get_traces() { return past_list; };
};

#endif
