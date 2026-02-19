#pragma once

#include <schedsim/core/event.hpp>

#include <map>

namespace schedsim::core {

class Engine;

/// @brief Provides O(1) timer cancellation by wrapping a map iterator.
/// @ingroup core_events
///
/// A TimerId is returned by Engine::set_timer() and stores an iterator into
/// the engine's ordered event map, enabling constant-time cancellation.
/// Default-constructed instances are invalid; only the Engine may create
/// valid identifiers.
///
/// Timer callbacks should call clear() at their entry point to prevent
/// dangling-iterator issues when the callback itself cancels other timers.
///
/// @see Engine::set_timer()
/// @see Engine::cancel_timer()
/// @see DeferredId
class TimerId {
    friend class Engine;

public:
    /// @brief Default-construct an invalid timer identifier.
    TimerId() = default;

    /// @brief Check whether this timer is still valid (not fired, not cancelled).
    /// @return `true` if the timer has not yet fired or been cancelled.
    [[nodiscard]] bool valid() const noexcept { return valid_; }

    /// @brief Allow contextual conversion to `bool` for `if (timer_id)` checks.
    /// @return `true` if valid.
    explicit operator bool() const noexcept { return valid_; }

    /// @brief Mark the timer as no longer valid.
    ///
    /// Call this at the start of timer callbacks to prevent dangling-iterator
    /// issues if the callback later tries to cancel this or another timer.
    void clear() noexcept { valid_ = false; }

private:
    using Iterator = std::map<EventKey, Event>::iterator;

    /// @brief Construct a valid TimerId from an event-map iterator (Engine-only).
    /// @param it Iterator pointing to the corresponding event in the map.
    TimerId(Iterator it) : it_(it), valid_(true) {}

    /// @brief Invalidate this timer (called by the Engine on cancellation or firing).
    void invalidate() noexcept { valid_ = false; }

    Iterator it_{};
    bool valid_{false};
};

} // namespace schedsim::core
