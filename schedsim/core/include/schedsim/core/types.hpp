#pragma once

#include <compare>
#include <cstdint>

namespace schedsim::core {

// Duration: represents a time interval in nanoseconds (int64_t).
// All construction via named factories. Duration/Duration returns double.
class Duration {
    int64_t ns_;

    explicit constexpr Duration(int64_t ns) noexcept : ns_(ns) {}

    // Round double seconds to nearest nanosecond
    static constexpr int64_t secs_to_ns(double s) noexcept {
        return static_cast<int64_t>(s * 1e9 + (s >= 0.0 ? 0.5 : -0.5));
    }

    // Round double seconds up to next nanosecond (for completion timers)
    static constexpr int64_t secs_to_ns_ceil(double s) noexcept {
        double ns_d = s * 1e9;
        auto ns_i = static_cast<int64_t>(ns_d);
        if (static_cast<double>(ns_i) < ns_d) {
            ++ns_i;
        }
        return ns_i;
    }

    // Bridge functions are friends so they can use the private constructor
    friend constexpr Duration duration_from_seconds(double s) noexcept;
    friend constexpr Duration duration_from_seconds_ceil(double s) noexcept;
    friend constexpr Duration duration_from_nanoseconds(int64_t ns) noexcept;
    friend constexpr double duration_to_seconds(Duration d) noexcept;
    friend constexpr int64_t duration_to_nanoseconds(Duration d) noexcept;
    friend constexpr Duration scale_duration(Duration d, double factor) noexcept;
    friend constexpr Duration divide_duration(Duration d, double divisor) noexcept;
    friend constexpr double duration_ratio(Duration a, Duration b) noexcept;

public:
    // Default constructor: zero (needed for aggregate init and uninitialized locals)
    constexpr Duration() noexcept : ns_(0) {}

    // Named factories
    static constexpr Duration zero() noexcept { return Duration{0}; }

    // Accessors (prefer bridge functions at boundaries)
    [[nodiscard]] constexpr double seconds() const noexcept {
        return static_cast<double>(ns_) * 1e-9;
    }
    [[nodiscard]] constexpr int64_t nanoseconds() const noexcept {
        return ns_;
    }

    // Arithmetic: Duration ↔ Duration
    constexpr Duration operator+(Duration rhs) const noexcept {
        return Duration{ns_ + rhs.ns_};
    }
    constexpr Duration operator-(Duration rhs) const noexcept {
        return Duration{ns_ - rhs.ns_};
    }
    constexpr Duration& operator+=(Duration rhs) noexcept {
        ns_ += rhs.ns_;
        return *this;
    }
    constexpr Duration& operator-=(Duration rhs) noexcept {
        ns_ -= rhs.ns_;
        return *this;
    }
    constexpr Duration operator-() const noexcept {
        return Duration{-ns_};
    }

    // Comparisons
    constexpr auto operator<=>(const Duration& rhs) const noexcept = default;
    constexpr bool operator==(const Duration& rhs) const noexcept = default;
};

// TimePoint: represents an absolute simulation time as Duration since epoch.
class TimePoint {
    Duration since_epoch_;

    explicit constexpr TimePoint(Duration d) noexcept : since_epoch_(d) {}

    friend constexpr TimePoint time_from_seconds(double s) noexcept;
    friend constexpr double time_to_seconds(TimePoint tp) noexcept;

public:
    // Default constructor: epoch (needed for containers like std::map/unordered_map)
    constexpr TimePoint() noexcept : since_epoch_(Duration::zero()) {}

    // Named factories
    static constexpr TimePoint epoch() noexcept {
        return TimePoint{Duration::zero()};
    }

    // Accessor
    [[nodiscard]] constexpr Duration time_since_epoch() const noexcept {
        return since_epoch_;
    }

    // Arithmetic: TimePoint ± Duration → TimePoint
    constexpr TimePoint operator+(Duration d) const noexcept {
        return TimePoint{since_epoch_ + d};
    }
    constexpr TimePoint operator-(Duration d) const noexcept {
        return TimePoint{since_epoch_ - d};
    }
    constexpr TimePoint& operator+=(Duration d) noexcept {
        since_epoch_ += d;
        return *this;
    }
    constexpr TimePoint& operator-=(Duration d) noexcept {
        since_epoch_ -= d;
        return *this;
    }

    // Arithmetic: TimePoint - TimePoint → Duration
    constexpr Duration operator-(TimePoint rhs) const noexcept {
        return since_epoch_ - rhs.since_epoch_;
    }

    // Comparisons
    constexpr auto operator<=>(const TimePoint& rhs) const noexcept = default;
    constexpr bool operator==(const TimePoint& rhs) const noexcept = default;
};

// Strong types for physical quantities (decision 45)
struct Frequency {
    double mhz;

    constexpr bool operator==(const Frequency&) const = default;
    constexpr auto operator<=>(const Frequency&) const = default;
};

struct Power {
    double mw;

    constexpr bool operator==(const Power&) const = default;
    constexpr auto operator<=>(const Power&) const = default;
};

struct Energy {
    double mj;

    constexpr bool operator==(const Energy&) const = default;
    constexpr auto operator<=>(const Energy&) const = default;

    constexpr Energy& operator+=(const Energy& other) {
        mj += other.mj;
        return *this;
    }
};

// ============================================================================
// Bridge functions — the canonical API for Duration/TimePoint conversion
// ============================================================================

[[nodiscard]] constexpr Duration duration_from_seconds(double s) noexcept {
    return Duration{Duration::secs_to_ns(s)};
}

[[nodiscard]] constexpr Duration duration_from_seconds_ceil(double s) noexcept {
    return Duration{Duration::secs_to_ns_ceil(s)};
}

[[nodiscard]] constexpr double duration_to_seconds(Duration d) noexcept {
    return d.seconds();
}

[[nodiscard]] constexpr Duration duration_from_nanoseconds(int64_t ns) noexcept {
    return Duration{ns};
}

[[nodiscard]] constexpr int64_t duration_to_nanoseconds(Duration d) noexcept {
    return d.nanoseconds();
}

[[nodiscard]] constexpr TimePoint time_from_seconds(double s) noexcept {
    return TimePoint{duration_from_seconds(s)};
}

[[nodiscard]] constexpr double time_to_seconds(TimePoint tp) noexcept {
    return tp.time_since_epoch().seconds();
}

// Scaled arithmetic: Duration * double → Duration (round to nearest)
[[nodiscard]] constexpr Duration scale_duration(Duration d, double factor) noexcept {
    return duration_from_seconds(d.seconds() * factor);
}

// Scaled arithmetic: Duration / double → Duration (round to nearest)
[[nodiscard]] constexpr Duration divide_duration(Duration d, double divisor) noexcept {
    return duration_from_seconds(d.seconds() / divisor);
}

// Ratio: Duration / Duration → double (NOT integer truncation)
[[nodiscard]] constexpr double duration_ratio(Duration a, Duration b) noexcept {
    return a.seconds() / b.seconds();
}

} // namespace schedsim::core
