#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/error.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

TEST(ClockDomainTest, Construction) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    EXPECT_EQ(cd.id(), 0U);
    EXPECT_EQ(cd.freq_min().mhz, 500.0);
    EXPECT_EQ(cd.freq_max().mhz, 2000.0);
    // Initial frequency is max
    EXPECT_EQ(cd.frequency().mhz, 2000.0);
    EXPECT_FALSE(cd.is_locked());
}

TEST(ClockDomainTest, ConstructionWithDelay) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0}, Duration{0.001});

    EXPECT_EQ(cd.transition_delay().count(), 0.001);
}

TEST(ClockDomainTest, SetFrequency) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    cd.set_frequency(Frequency{1000.0});
    EXPECT_EQ(cd.frequency().mhz, 1000.0);

    cd.set_frequency(Frequency{500.0});
    EXPECT_EQ(cd.frequency().mhz, 500.0);

    cd.set_frequency(Frequency{2000.0});
    EXPECT_EQ(cd.frequency().mhz, 2000.0);
}

TEST(ClockDomainTest, SetFrequencyOutOfRangeThrows) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    EXPECT_THROW(cd.set_frequency(Frequency{400.0}), OutOfRangeError);
    EXPECT_THROW(cd.set_frequency(Frequency{2500.0}), OutOfRangeError);
}

TEST(ClockDomainTest, LockFrequency) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    cd.set_frequency(Frequency{1000.0});
    cd.lock_frequency();

    EXPECT_TRUE(cd.is_locked());
    EXPECT_EQ(cd.frequency().mhz, 1000.0);
}

TEST(ClockDomainTest, SetFrequencyOnLockedThrows) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    cd.lock_frequency();

    EXPECT_THROW(cd.set_frequency(Frequency{1500.0}), InvalidStateError);
}

TEST(ClockDomainTest, ProcessorsInitiallyEmpty) {
    ClockDomain cd(0, Frequency{500.0}, Frequency{2000.0});

    EXPECT_TRUE(cd.processors().empty());
}

TEST(ClockDomainTest, MoveConstruction) {
    ClockDomain cd1(0, Frequency{500.0}, Frequency{2000.0});
    cd1.set_frequency(Frequency{1000.0});

    ClockDomain cd2(std::move(cd1));

    EXPECT_EQ(cd2.id(), 0U);
    EXPECT_EQ(cd2.frequency().mhz, 1000.0);
    EXPECT_EQ(cd2.freq_min().mhz, 500.0);
    EXPECT_EQ(cd2.freq_max().mhz, 2000.0);
}
