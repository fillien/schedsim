#include <schedsim/core/types.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace schedsim::core;

// ============================================================================
// Duration tests
// ============================================================================

TEST(DurationTest, Factories) {
    Duration d1 = duration_from_seconds(1.5);
    EXPECT_DOUBLE_EQ(duration_to_seconds(d1), 1.5);

    Duration d2 = duration_from_nanoseconds(500);
    EXPECT_EQ(duration_to_nanoseconds(d2), 500);

    Duration d3 = duration_from_seconds_ceil(1.0);
    EXPECT_EQ(duration_to_nanoseconds(d3), 1'000'000'000);

    Duration d4 = Duration::zero();
    EXPECT_EQ(duration_to_nanoseconds(d4), 0);

    Duration d5{};
    EXPECT_EQ(duration_to_nanoseconds(d5), 0);
}

TEST(DurationTest, ZeroEquivalence) {
    EXPECT_EQ(Duration::zero(), Duration{});
    EXPECT_EQ(duration_from_seconds(0.0), Duration::zero());
    EXPECT_EQ(duration_from_nanoseconds(0), Duration::zero());
}

TEST(DurationTest, RoundTrip) {
    for (double v : {0.001, 1.0, 100.0, 1e-9}) {
        EXPECT_DOUBLE_EQ(duration_to_seconds(duration_from_seconds(v)), v);
    }
}

TEST(DurationTest, Arithmetic) {
    Duration a = duration_from_seconds(5.0);
    Duration b = duration_from_seconds(3.0);

    EXPECT_DOUBLE_EQ(duration_to_seconds(a + b), 8.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(a - b), 2.0);

    Duration c = a;
    c += b;
    EXPECT_DOUBLE_EQ(duration_to_seconds(c), 8.0);

    Duration d = a;
    d -= b;
    EXPECT_DOUBLE_EQ(duration_to_seconds(d), 2.0);

    EXPECT_DOUBLE_EQ(duration_to_seconds(-a), -5.0);
}

TEST(DurationTest, Scaling) {
    Duration d = duration_from_seconds(3.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(scale_duration(d, 2.0)), 6.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(divide_duration(d, 3.0)), 1.0);

    // Edge cases
    EXPECT_EQ(scale_duration(Duration::zero(), 100.0), Duration::zero());
    EXPECT_EQ(scale_duration(d, 1.0), d);
    EXPECT_DOUBLE_EQ(duration_to_seconds(scale_duration(d, -1.0)), -3.0);
}

TEST(DurationTest, Ratio) {
    Duration a = duration_from_seconds(6.0);
    Duration b = duration_from_seconds(3.0);

    // Returns double, not truncated
    EXPECT_DOUBLE_EQ(duration_ratio(a, b), 2.0);
    EXPECT_DOUBLE_EQ(duration_ratio(b, a), 0.5);
    EXPECT_DOUBLE_EQ(duration_ratio(a, a), 1.0);
    EXPECT_DOUBLE_EQ(duration_ratio(-a, a), -1.0);
}

TEST(DurationTest, Comparisons) {
    Duration pos = duration_from_seconds(1.0);
    Duration neg = duration_from_seconds(-1.0);
    Duration zero = Duration::zero();

    EXPECT_LT(neg, zero);
    EXPECT_LT(zero, pos);
    EXPECT_LT(neg, pos);

    EXPECT_GT(pos, zero);
    EXPECT_GT(zero, neg);

    EXPECT_LE(zero, zero);
    EXPECT_LE(neg, zero);
    EXPECT_GE(zero, zero);
    EXPECT_GE(pos, zero);

    EXPECT_EQ(zero, Duration::zero());
    EXPECT_NE(pos, neg);
}

TEST(DurationTest, Negative) {
    Duration neg = duration_from_seconds(-2.0);
    EXPECT_DOUBLE_EQ(duration_to_seconds(neg), -2.0);
    EXPECT_LT(neg, Duration::zero());

    Duration pos = -neg;
    EXPECT_DOUBLE_EQ(duration_to_seconds(pos), 2.0);

    Duration sum = neg + pos;
    EXPECT_EQ(sum, Duration::zero());
}

TEST(DurationTest, Rounding) {
    // secs_to_ns rounds to nearest: 1.5ns → 2, 0.4ns → 0, -1.5ns → -2
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds(1.5e-9)), 2);
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds(0.4e-9)), 0);
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds(-1.5e-9)), -2);
}

TEST(DurationTest, Ceil) {
    // duration_from_seconds_ceil: 1.1ns → 2, 1.0ns → 1, 0.0 → 0
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds_ceil(1.1e-9)), 2);
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds_ceil(1.0e-9)), 1);
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds_ceil(0.0)), 0);

    // Negative input: truncation toward zero = ceiling for negatives
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds_ceil(-1.1e-9)), -1);
}

TEST(DurationTest, NanosecondAccess) {
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds(1.0)), 1'000'000'000);
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds(0.5)), 500'000'000);
    EXPECT_EQ(duration_to_nanoseconds(duration_from_nanoseconds(1)), 1);
}

TEST(DurationTest, LargeValues) {
    double large = 1e6;  // 1 million seconds
    EXPECT_DOUBLE_EQ(duration_to_seconds(duration_from_seconds(large)), large);
}

TEST(DurationTest, Constexpr) {
    static_assert(Duration::zero() == Duration{});
    static_assert(duration_from_seconds(1.0) + duration_from_seconds(2.0) == duration_from_seconds(3.0));
    static_assert(duration_from_nanoseconds(500) == duration_from_nanoseconds(500));
    static_assert(duration_from_seconds(1.0) > Duration::zero());
    static_assert(Duration::zero() < duration_from_seconds(1.0));
}

TEST(DurationTest, DivideDurationByOne) {
    Duration d = duration_from_seconds(3.5);
    EXPECT_EQ(divide_duration(d, 1.0), d);
}

TEST(DurationTest, OverflowBoundary) {
    // 9e9 seconds = 9e18 nanoseconds, well within int64_t max (~9.22e18)
    double large_s = 9e9;
    Duration d = duration_from_seconds(large_s);
    EXPECT_DOUBLE_EQ(duration_to_seconds(d), large_s);
}

TEST(DurationTest, RatioByZero) {
    // duration_ratio divides seconds (doubles), so division by zero follows IEEE 754
    Duration pos = duration_from_seconds(1.0);
    double result = duration_ratio(pos, Duration::zero());
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0.0);
}

TEST(DurationTest, SubNanosecondBoundary) {
    // 0.5ns is the exact tie-break boundary — rounds to 1ns (round-half-away-from-zero)
    EXPECT_EQ(duration_to_nanoseconds(duration_from_seconds(0.5e-9)), 1);
}

// ============================================================================
// TimePoint tests
// ============================================================================

TEST(TimePointTest, Factories) {
    TimePoint t1 = time_from_seconds(5.0);
    EXPECT_DOUBLE_EQ(time_to_seconds(t1), 5.0);

    TimePoint t2 = TimePoint::epoch();
    EXPECT_DOUBLE_EQ(time_to_seconds(t2), 0.0);

    TimePoint t3{};
    EXPECT_DOUBLE_EQ(time_to_seconds(t3), 0.0);
}

TEST(TimePointTest, EpochEquivalence) {
    EXPECT_EQ(TimePoint::epoch(), TimePoint{});
    EXPECT_EQ(time_from_seconds(0.0), TimePoint{});
}

TEST(TimePointTest, Arithmetic) {
    TimePoint t = time_from_seconds(10.0);
    Duration d = duration_from_seconds(5.0);

    TimePoint t2 = t + d;
    EXPECT_DOUBLE_EQ(time_to_seconds(t2), 15.0);

    TimePoint t3 = t - d;
    EXPECT_DOUBLE_EQ(time_to_seconds(t3), 5.0);

    TimePoint t4 = t;
    t4 += d;
    EXPECT_DOUBLE_EQ(time_to_seconds(t4), 15.0);

    TimePoint t5 = t;
    t5 -= d;
    EXPECT_DOUBLE_EQ(time_to_seconds(t5), 5.0);

    Duration diff = t2 - t;
    EXPECT_DOUBLE_EQ(duration_to_seconds(diff), 5.0);
}

TEST(TimePointTest, Comparisons) {
    TimePoint a = time_from_seconds(1.0);
    TimePoint b = time_from_seconds(2.0);
    TimePoint c = time_from_seconds(1.0);

    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
    EXPECT_EQ(a, c);
    EXPECT_LE(a, c);
    EXPECT_GE(b, a);
    EXPECT_NE(a, b);
}

TEST(TimePointTest, TimeSinceEpoch) {
    TimePoint t = time_from_seconds(3.5);
    Duration d = t.time_since_epoch();
    EXPECT_DOUBLE_EQ(duration_to_seconds(d), 3.5);

    EXPECT_EQ(TimePoint::epoch().time_since_epoch(), Duration::zero());
}

TEST(TimePointTest, RoundTrip) {
    for (double v : {0.0, 0.001, 1.0, 100.0, 1e-9}) {
        EXPECT_DOUBLE_EQ(time_to_seconds(time_from_seconds(v)), v);
    }
}

// ============================================================================
// Frequency and Energy tests (kept from original)
// ============================================================================

TEST(TypesTest, FrequencyComparison) {
    Frequency f1{1000.0};
    Frequency f2{2000.0};

    EXPECT_LT(f1, f2);
    EXPECT_EQ(f1, Frequency{1000.0});
}

TEST(TypesTest, EnergyAccumulation) {
    Energy e1{10.0};
    Energy e2{5.0};

    e1 += e2;
    EXPECT_DOUBLE_EQ(e1.mj, 15.0);
}
