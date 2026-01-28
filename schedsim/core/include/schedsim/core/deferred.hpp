#pragma once

#include <cstddef>
#include <cstdint>

namespace schedsim::core {

class Engine;

// DeferredId identifies a registered deferred callback (Decision 87)
// Wraps an index into the deferred callbacks vector
class DeferredId {
    friend class Engine;

public:
    DeferredId() = default;

    // Check if the deferred callback ID is valid
    [[nodiscard]] bool valid() const noexcept { return valid_; }

    // Allow implicit conversion to bool for if(deferred_id) checks
    explicit operator bool() const noexcept { return valid_; }

private:
    explicit DeferredId(std::size_t index) : index_(index), valid_(true) {}

    std::size_t index_{0};
    bool valid_{false};
};

} // namespace schedsim::core
