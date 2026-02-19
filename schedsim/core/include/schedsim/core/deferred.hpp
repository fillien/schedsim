#pragma once

#include <cstddef>
#include <cstdint>

namespace schedsim::core {

class Engine;

/// @brief Identifies a registered deferred callback.
/// @ingroup core_events
///
/// A DeferredId is returned by Engine::defer() and wraps an index into the
/// engine's internal deferred-callback vector. Default-constructed instances
/// are invalid; only the Engine may create valid identifiers.
///
/// @see Engine::defer()
/// @see Engine::cancel_deferred()
class DeferredId {
    friend class Engine;

public:
    /// @brief Default-construct an invalid deferred callback identifier.
    DeferredId() = default;

    /// @brief Check whether this identifier refers to a valid deferred callback.
    /// @return `true` if the identifier was created by the Engine and has not
    ///         been invalidated, `false` otherwise.
    [[nodiscard]] bool valid() const noexcept { return valid_; }

    /// @brief Allow contextual conversion to `bool` for `if (deferred_id)` checks.
    /// @return `true` if valid.
    explicit operator bool() const noexcept { return valid_; }

private:
    /// @brief Construct a valid DeferredId (Engine-only).
    /// @param index Position in the deferred-callback vector.
    explicit DeferredId(std::size_t index) : index_(index), valid_(true) {}

    std::size_t index_{0};
    bool valid_{false};
};

} // namespace schedsim::core
