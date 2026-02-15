#include <schedsim/core/clock_domain.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

TEST(ClockDomainModesTest, NoModesIsContinuous) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    EXPECT_FALSE(cd.has_frequency_modes());
    EXPECT_TRUE(cd.frequency_modes().empty());
}

TEST(ClockDomainModesTest, SetFrequencyModesSortsAndDeduplicates) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    cd.set_frequency_modes({Frequency{1500.0}, Frequency{500.0}, Frequency{1000.0},
                            Frequency{1500.0}, Frequency{2000.0}});

    ASSERT_TRUE(cd.has_frequency_modes());
    auto modes = cd.frequency_modes();
    ASSERT_EQ(modes.size(), 4U);
    EXPECT_DOUBLE_EQ(modes[0].mhz, 500.0);
    EXPECT_DOUBLE_EQ(modes[1].mhz, 1000.0);
    EXPECT_DOUBLE_EQ(modes[2].mhz, 1500.0);
    EXPECT_DOUBLE_EQ(modes[3].mhz, 2000.0);
}

TEST(ClockDomainModesTest, SetFrequencyModesUpdatesMinMax) {
    ClockDomain cd(0, Frequency{100.0}, Frequency{3000.0});

    cd.set_frequency_modes({Frequency{500.0}, Frequency{1000.0}, Frequency{2000.0}});

    EXPECT_DOUBLE_EQ(cd.freq_min().mhz, 500.0);
    EXPECT_DOUBLE_EQ(cd.freq_max().mhz, 2000.0);
}

TEST(ClockDomainModesTest, CeilToModeDiscrete) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});
    cd.set_frequency_modes({Frequency{500.0}, Frequency{1000.0}, Frequency{1500.0},
                            Frequency{2000.0}});

    // Exact match
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{1000.0}).mhz, 1000.0);

    // Between modes: rounds up
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{750.0}).mhz, 1000.0);
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{1001.0}).mhz, 1500.0);

    // At minimum
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{500.0}).mhz, 500.0);

    // Below minimum: returns lowest mode
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{100.0}).mhz, 500.0);

    // Above maximum: returns max mode
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{3000.0}).mhz, 2000.0);
}

TEST(ClockDomainModesTest, CeilToModeContinuous) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    // Continuous mode: clamps to [min, max]
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{1000.0}).mhz, 1000.0);
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{100.0}).mhz, 500.0);
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{3000.0}).mhz, 2000.0);
}

TEST(ClockDomainModesTest, FreqEffDefaultZero) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    EXPECT_DOUBLE_EQ(cd.freq_eff().mhz, 0.0);
}

TEST(ClockDomainModesTest, FreqEffSetGet) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    cd.set_freq_eff(Frequency{1000.0});
    EXPECT_DOUBLE_EQ(cd.freq_eff().mhz, 1000.0);
}

TEST(ClockDomainModesTest, SingleMode) {
    ClockDomain cd(0, Frequency{1000.0}, Frequency{1000.0});
    cd.set_frequency_modes({Frequency{1000.0}});

    ASSERT_TRUE(cd.has_frequency_modes());
    EXPECT_EQ(cd.frequency_modes().size(), 1U);
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{500.0}).mhz, 1000.0);
    EXPECT_DOUBLE_EQ(cd.ceil_to_mode(Frequency{2000.0}).mhz, 1000.0);
}
