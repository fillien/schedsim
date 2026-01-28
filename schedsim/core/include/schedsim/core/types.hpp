#pragma once

#include <chrono>
#include <ratio>

namespace schedsim::core {

// Custom clock for simulation time (decision 45, 49)
struct SimClock {
    using rep        = double;
    using period     = std::ratio<1>;  // 1 second base unit
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<SimClock>;
    static constexpr bool is_steady = true;
};

using Duration  = SimClock::duration;
using TimePoint = SimClock::time_point;

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

} // namespace schedsim::core
