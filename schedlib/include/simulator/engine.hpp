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

/**
 * @brief The Engine class simulates events over time on a given platform
 *        and dispatches them to an attached scheduler.
 */
class Engine {
      public:
        static constexpr double ZERO_ROUNDED = 0.0000001;

        explicit Engine(bool is_there_delay);

        auto scheduler(std::unique_ptr<allocators::Allocator> alloc) -> void
        {
                alloc_ = std::move(alloc);
        }

        auto platform(std::unique_ptr<Platform> plat) -> void { platform_ = std::move(plat); }

        auto simulation() -> void;

        [[nodiscard]] auto chip() -> Platform& { return *platform_; }
        [[nodiscard]] auto chip() const -> const Platform& { return *platform_; }

        [[nodiscard]] auto time() const -> double { return current_timestamp_; }

        [[nodiscard]] auto future_list() const -> const std::multimap<double, events::Event>&
        {
                return future_list_;
        }

        auto remove_event(const std::function<bool(const std::pair<double, events::Event>&)>& pred)
            -> std::size_t
        {
                return std::erase_if(future_list_, pred);
        }

        auto add_event(const events::Event& new_event, const double& timestamp) -> void;
        auto add_trace(const protocols::traces::trace& new_trace) -> void;

        static auto round_zero(const double& value) -> double
        {
                return (value >= -ZERO_ROUNDED && value <= ZERO_ROUNDED) ? 0.0 : value;
        }

        [[nodiscard]] auto traces() const
            -> const std::vector<std::pair<double, protocols::traces::trace>>&
        {
                return past_list_;
        }

        [[nodiscard]] auto alloc() -> allocators::Allocator& { return *alloc_; }
        [[nodiscard]] auto alloc() const -> const allocators::Allocator& { return *alloc_; }

        [[nodiscard]] auto is_delay_activated() const -> bool { return delay_activated_; }

      private:
        double current_timestamp_{0};
        std::unique_ptr<allocators::Allocator> alloc_;
        std::unique_ptr<Platform> platform_;
        std::vector<std::pair<double, protocols::traces::trace>> past_list_;
        bool delay_activated_{false};
        std::multimap<double, events::Event> future_list_;
};

#endif // ENGINE_HPP
