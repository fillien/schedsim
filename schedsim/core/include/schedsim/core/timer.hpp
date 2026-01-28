#pragma once

#include <schedsim/core/event.hpp>

#include <map>

namespace schedsim::core {

class Engine;

// TimerId provides O(1) timer cancellation (Decision 52)
// Wraps a map iterator with a validity flag
class TimerId {
    friend class Engine;

public:
    TimerId() = default;

    // Check if the timer is still valid (not fired, not cancelled)
    [[nodiscard]] bool valid() const noexcept { return valid_; }

    // Allow implicit conversion to bool for if(timer_id) checks
    explicit operator bool() const noexcept { return valid_; }

    // Mark the timer as no longer valid.
    // Call this at the start of timer callbacks to prevent dangling iterator
    // issues if the callback later tries to cancel this or another timer.
    void clear() noexcept { valid_ = false; }

private:
    using Iterator = std::map<EventKey, Event>::iterator;

    TimerId(Iterator it) : it_(it), valid_(true) {}

    void invalidate() noexcept { valid_ = false; }

    Iterator it_{};
    bool valid_{false};
};

} // namespace schedsim::core
