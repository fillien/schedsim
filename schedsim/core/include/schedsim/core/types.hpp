#pragma once

#include <compare>
#include <cstdint>

namespace schedsim::core {

/// @brief Time interval represented as an integer nanosecond count.
///
/// Duration wraps an `int64_t` nanosecond value with a private constructor.
/// All construction goes through named factories or bridge functions, ensuring
/// explicit conversions between seconds (double) and nanoseconds (int64_t).
///
/// Arithmetic between two Durations yields a Duration; dividing Duration by
/// Duration yields a `double` (via `duration_ratio`), not an integer.
///
/// @see duration_from_seconds, duration_from_nanoseconds, duration_to_seconds
/// @see TimePoint
/// @ingroup core_types
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
    /// @brief Default constructor: zero duration.
    ///
    /// Needed for aggregate initialization and container value types.
    constexpr Duration() noexcept : ns_(0) {}

    /// @brief Named factory returning a zero-length duration.
    /// @return Duration with value 0 ns.
    static constexpr Duration zero() noexcept { return Duration{0}; }

    /// @brief Convert to seconds (double).
    /// @return Duration value in seconds.
    /// @see duration_to_seconds
    [[nodiscard]] constexpr double seconds() const noexcept {
        return static_cast<double>(ns_) * 1e-9;
    }

    /// @brief Return the raw nanosecond count.
    /// @return Duration value in nanoseconds.
    /// @see duration_to_nanoseconds
    [[nodiscard]] constexpr int64_t nanoseconds() const noexcept {
        return ns_;
    }

    /// @brief Add two durations.
    /// @param rhs Duration to add.
    /// @return Sum of the two durations.
    constexpr Duration operator+(Duration rhs) const noexcept {
        return Duration{ns_ + rhs.ns_};
    }

    /// @brief Subtract a duration.
    /// @param rhs Duration to subtract.
    /// @return Difference of the two durations.
    constexpr Duration operator-(Duration rhs) const noexcept {
        return Duration{ns_ - rhs.ns_};
    }

    /// @brief Add a duration in place.
    /// @param rhs Duration to add.
    /// @return Reference to this.
    constexpr Duration& operator+=(Duration rhs) noexcept {
        ns_ += rhs.ns_;
        return *this;
    }

    /// @brief Subtract a duration in place.
    /// @param rhs Duration to subtract.
    /// @return Reference to this.
    constexpr Duration& operator-=(Duration rhs) noexcept {
        ns_ -= rhs.ns_;
        return *this;
    }

    /// @brief Negate the duration.
    /// @return Duration with opposite sign.
    constexpr Duration operator-() const noexcept {
        return Duration{-ns_};
    }

    /// @brief Three-way comparison (defaulted).
    constexpr auto operator<=>(const Duration& rhs) const noexcept = default;

    /// @brief Equality comparison (defaulted).
    constexpr bool operator==(const Duration& rhs) const noexcept = default;
};

/// @brief Absolute simulation time as a Duration offset from epoch (time zero).
///
/// TimePoint supports arithmetic with Duration (TimePoint +/- Duration yields
/// TimePoint) and differencing (TimePoint - TimePoint yields Duration). Two
/// TimePoints cannot be added.
///
/// @see time_from_seconds, time_to_seconds, Duration
/// @ingroup core_types
class TimePoint {
    Duration since_epoch_;

    explicit constexpr TimePoint(Duration d) noexcept : since_epoch_(d) {}

    friend constexpr TimePoint time_from_seconds(double s) noexcept;
    friend constexpr double time_to_seconds(TimePoint tp) noexcept;

public:
    /// @brief Default constructor: epoch (time zero).
    ///
    /// Needed for containers like `std::map` and `std::unordered_map`.
    constexpr TimePoint() noexcept : since_epoch_(Duration::zero()) {}

    /// @brief Named factory returning the epoch (time zero).
    /// @return TimePoint at simulation start.
    static constexpr TimePoint epoch() noexcept {
        return TimePoint{Duration::zero()};
    }

    /// @brief Return the duration elapsed since epoch.
    /// @return Duration from epoch to this time point.
    [[nodiscard]] constexpr Duration time_since_epoch() const noexcept {
        return since_epoch_;
    }

    /// @brief Advance a time point by a duration.
    /// @param d Duration to add.
    /// @return New time point offset by @p d.
    constexpr TimePoint operator+(Duration d) const noexcept {
        return TimePoint{since_epoch_ + d};
    }

    /// @brief Retreat a time point by a duration.
    /// @param d Duration to subtract.
    /// @return New time point offset backward by @p d.
    constexpr TimePoint operator-(Duration d) const noexcept {
        return TimePoint{since_epoch_ - d};
    }

    /// @brief Advance this time point in place.
    /// @param d Duration to add.
    /// @return Reference to this.
    constexpr TimePoint& operator+=(Duration d) noexcept {
        since_epoch_ += d;
        return *this;
    }

    /// @brief Retreat this time point in place.
    /// @param d Duration to subtract.
    /// @return Reference to this.
    constexpr TimePoint& operator-=(Duration d) noexcept {
        since_epoch_ -= d;
        return *this;
    }

    /// @brief Compute the duration between two time points.
    /// @param rhs Time point to subtract.
    /// @return Duration from @p rhs to this time point.
    constexpr Duration operator-(TimePoint rhs) const noexcept {
        return since_epoch_ - rhs.since_epoch_;
    }

    /// @brief Three-way comparison (defaulted).
    constexpr auto operator<=>(const TimePoint& rhs) const noexcept = default;

    /// @brief Equality comparison (defaulted).
    constexpr bool operator==(const TimePoint& rhs) const noexcept = default;
};

/// @brief Strong type for processor clock frequency in MHz.
/// @ingroup core_types
struct Frequency {
    double mhz; ///< Frequency value in megahertz.

    constexpr bool operator==(const Frequency&) const = default;
    constexpr auto operator<=>(const Frequency&) const = default;
};

/// @brief Strong type for power consumption in milliwatts.
/// @ingroup core_types
struct Power {
    double mw; ///< Power value in milliwatts.

    constexpr bool operator==(const Power&) const = default;
    constexpr auto operator<=>(const Power&) const = default;
};

/// @brief Strong type for energy consumption in millijoules.
/// @ingroup core_types
struct Energy {
    double mj; ///< Energy value in millijoules.

    constexpr bool operator==(const Energy&) const = default;
    constexpr auto operator<=>(const Energy&) const = default;

    /// @brief Accumulate energy.
    /// @param other Energy to add.
    /// @return Reference to this.
    constexpr Energy& operator+=(const Energy& other) {
        mj += other.mj;
        return *this;
    }
};

// ============================================================================
// Bridge functions — the canonical API for Duration/TimePoint conversion
// ============================================================================

/// @brief Create a Duration from a value in seconds (round to nearest ns).
/// @param s Time interval in seconds.
/// @return Duration rounded to the nearest nanosecond.
/// @see duration_from_seconds_ceil, duration_to_seconds
[[nodiscard]] constexpr Duration duration_from_seconds(double s) noexcept {
    return Duration{Duration::secs_to_ns(s)};
}

/// @brief Create a Duration from a value in seconds (round up to next ns).
///
/// Ceiling rounding is used for completion timers where truncation could
/// cause a timer to fire one nanosecond too early.
///
/// @param s Time interval in seconds.
/// @return Duration rounded up to the next nanosecond.
/// @see duration_from_seconds
[[nodiscard]] constexpr Duration duration_from_seconds_ceil(double s) noexcept {
    return Duration{Duration::secs_to_ns_ceil(s)};
}

/// @brief Convert a Duration to seconds (double).
/// @param d Duration to convert.
/// @return Value in seconds.
/// @see duration_from_seconds
[[nodiscard]] constexpr double duration_to_seconds(Duration d) noexcept {
    return d.seconds();
}

/// @brief Create a Duration from a raw nanosecond count.
/// @param ns Nanosecond count.
/// @return Duration wrapping @p ns.
/// @see duration_to_nanoseconds
[[nodiscard]] constexpr Duration duration_from_nanoseconds(int64_t ns) noexcept {
    return Duration{ns};
}

/// @brief Extract the raw nanosecond count from a Duration.
/// @param d Duration to convert.
/// @return Nanosecond count.
/// @see duration_from_nanoseconds
[[nodiscard]] constexpr int64_t duration_to_nanoseconds(Duration d) noexcept {
    return d.nanoseconds();
}

/// @brief Create a TimePoint from a value in seconds since epoch.
/// @param s Absolute time in seconds.
/// @return TimePoint at @p s seconds from epoch.
/// @see time_to_seconds
[[nodiscard]] constexpr TimePoint time_from_seconds(double s) noexcept {
    return TimePoint{duration_from_seconds(s)};
}

/// @brief Convert a TimePoint to seconds since epoch (double).
/// @param tp TimePoint to convert.
/// @return Absolute time in seconds.
/// @see time_from_seconds
[[nodiscard]] constexpr double time_to_seconds(TimePoint tp) noexcept {
    return tp.time_since_epoch().seconds();
}

/// @brief Scale a Duration by a floating-point factor (round to nearest ns).
///
/// Useful for frequency/performance scaling of execution times.
///
/// @param d Duration to scale.
/// @param factor Multiplicative factor.
/// @return Scaled duration, rounded to nearest nanosecond.
/// @see divide_duration, duration_ratio
[[nodiscard]] constexpr Duration scale_duration(Duration d, double factor) noexcept {
    return duration_from_seconds(d.seconds() * factor);
}

/// @brief Divide a Duration by a floating-point divisor (round to nearest ns).
///
/// Useful for computing remaining execution time at a different speed.
///
/// @param d Duration to divide.
/// @param divisor Divisor (must not be zero).
/// @return Divided duration, rounded to nearest nanosecond.
/// @see scale_duration, duration_ratio
[[nodiscard]] constexpr Duration divide_duration(Duration d, double divisor) noexcept {
    return duration_from_seconds(d.seconds() / divisor);
}

/// @brief Compute the ratio of two Durations as a double.
///
/// Returns `a / b` as a floating-point value, not an integer truncation.
/// This is the canonical way to compute utilization ratios (WCET/period).
///
/// @param a Numerator duration.
/// @param b Denominator duration (must not be zero).
/// @return Floating-point ratio a/b.
/// @see scale_duration, divide_duration
[[nodiscard]] constexpr double duration_ratio(Duration a, Duration b) noexcept {
    return a.seconds() / b.seconds();
}

} // namespace schedsim::core
