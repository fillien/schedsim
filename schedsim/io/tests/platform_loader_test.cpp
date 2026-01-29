#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/error.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>

#include <gtest/gtest.h>

using namespace schedsim::io;
using namespace schedsim::core;

class PlatformLoaderTest : public ::testing::Test {
protected:
    Engine engine;
};

// =============================================================================
// New Format Tests
// =============================================================================

TEST_F(PlatformLoaderTest, LoadNewFormatBasic) {
    const char* json = R"({
        "processor_types": [
            {"name": "big", "performance": 1.0}
        ],
        "clock_domains": [{
            "id": 0,
            "frequencies_mhz": [200, 400, 600, 800, 1000, 1200],
            "initial_frequency_mhz": 1200
        }],
        "power_domains": [{
            "id": 0,
            "c_states": [
                {"level": 0, "power_mw": 100, "latency_us": 0, "scope": "per_processor"}
            ]
        }],
        "processors": [
            {"id": 0, "type": "big", "clock_domain": 0, "power_domain": 0}
        ]
    })";

    load_platform_from_string(engine, json);

    auto& platform = engine.platform();
    EXPECT_EQ(platform.processor_type_count(), 1u);
    EXPECT_EQ(platform.clock_domain_count(), 1u);
    EXPECT_EQ(platform.power_domain_count(), 1u);
    EXPECT_EQ(platform.processor_count(), 1u);

    EXPECT_EQ(platform.processor_type(0).name(), "big");
    EXPECT_DOUBLE_EQ(platform.processor_type(0).performance(), 1.0);

    EXPECT_DOUBLE_EQ(platform.clock_domain(0).frequency().mhz, 1200.0);
    EXPECT_DOUBLE_EQ(platform.clock_domain(0).freq_min().mhz, 200.0);
    EXPECT_DOUBLE_EQ(platform.clock_domain(0).freq_max().mhz, 1200.0);
}

TEST_F(PlatformLoaderTest, LoadNewFormatWithPowerModel) {
    const char* json = R"({
        "processor_types": [
            {"name": "cpu", "performance": 1.0}
        ],
        "clock_domains": [{
            "id": 0,
            "frequencies_mhz": [500, 1000],
            "power_model": [0.044, 3.4e-6, 2.2e-8, 4.6e-11]
        }],
        "power_domains": [{
            "id": 0,
            "c_states": [{"level": 0, "power_mw": 100, "latency_us": 0}]
        }],
        "processors": [
            {"type": "cpu", "clock_domain": 0, "power_domain": 0}
        ]
    })";

    load_platform_from_string(engine, json);
    auto& cd = engine.platform().clock_domain(0);

    // Verify power model is set by checking power at a frequency
    // Power should be non-zero if model is set correctly
    auto power = cd.power_at_frequency(Frequency{1.0});  // 1 GHz
    EXPECT_GT(power.mw, 0.0);
}

TEST_F(PlatformLoaderTest, LoadNewFormatMultipleProcessors) {
    const char* json = R"({
        "processor_types": [
            {"name": "big", "performance": 1.0},
            {"name": "LITTLE", "performance": 0.3}
        ],
        "clock_domains": [
            {"id": 0, "frequencies_mhz": [1000, 2000]},
            {"id": 1, "frequencies_mhz": [500, 1000]}
        ],
        "power_domains": [
            {"id": 0, "c_states": [{"level": 0, "power_mw": 100, "latency_us": 0}]},
            {"id": 1, "c_states": [{"level": 0, "power_mw": 50, "latency_us": 0}]}
        ],
        "processors": [
            {"type": "big", "clock_domain": 0, "power_domain": 0},
            {"type": "big", "clock_domain": 0, "power_domain": 0},
            {"type": "LITTLE", "clock_domain": 1, "power_domain": 1},
            {"type": "LITTLE", "clock_domain": 1, "power_domain": 1}
        ]
    })";

    load_platform_from_string(engine, json);

    auto& platform = engine.platform();
    EXPECT_EQ(platform.processor_type_count(), 2u);
    EXPECT_EQ(platform.clock_domain_count(), 2u);
    EXPECT_EQ(platform.power_domain_count(), 2u);
    EXPECT_EQ(platform.processor_count(), 4u);
}

TEST_F(PlatformLoaderTest, LoadNewFormatDefaultCState) {
    const char* json = R"({
        "processor_types": [{"name": "cpu", "performance": 1.0}],
        "clock_domains": [{"id": 0, "frequencies_mhz": [1000]}],
        "power_domains": [{"id": 0}],
        "processors": [{"type": "cpu", "clock_domain": 0, "power_domain": 0}]
    })";

    load_platform_from_string(engine, json);

    auto& pd = engine.platform().power_domain(0);
    ASSERT_EQ(pd.c_states().size(), 1u);
    EXPECT_EQ(pd.c_states()[0].level, 0);
}

// =============================================================================
// Legacy Format Tests
// =============================================================================

TEST_F(PlatformLoaderTest, LoadLegacyFormatBasic) {
    const char* json = R"({
        "clusters": [
            {
                "procs": 4,
                "perf_score": 1.0,
                "effective_freq": 1200.0,
                "frequencies": [200.0, 400.0, 600.0, 800.0, 1000.0, 1200.0]
            }
        ]
    })";

    load_platform_from_string(engine, json);

    auto& platform = engine.platform();
    EXPECT_EQ(platform.processor_count(), 4u);
    EXPECT_EQ(platform.clock_domain_count(), 1u);
    EXPECT_EQ(platform.power_domain_count(), 1u);

    EXPECT_DOUBLE_EQ(platform.clock_domain(0).frequency().mhz, 1200.0);
}

TEST_F(PlatformLoaderTest, LoadLegacyFormatMultipleClusters) {
    const char* json = R"({
        "clusters": [
            {"procs": 4, "perf_score": 1.0, "effective_freq": 1800.0, "frequencies": [600, 1200, 1800]},
            {"procs": 4, "perf_score": 0.5, "effective_freq": 1200.0, "frequencies": [400, 800, 1200]}
        ]
    })";

    load_platform_from_string(engine, json);

    auto& platform = engine.platform();
    EXPECT_EQ(platform.processor_count(), 8u);
    EXPECT_EQ(platform.clock_domain_count(), 2u);
    EXPECT_EQ(platform.processor_type_count(), 2u);

    EXPECT_DOUBLE_EQ(platform.processor_type(0).performance(), 1.0);
    EXPECT_DOUBLE_EQ(platform.processor_type(1).performance(), 0.5);
}

TEST_F(PlatformLoaderTest, LoadLegacyFormatWithPowerModel) {
    const char* json = R"({
        "clusters": [
            {
                "procs": 1,
                "effective_freq": 1000.0,
                "frequencies": [1000.0],
                "power_model": [0.1, 0.0, 0.0, 0.0]
            }
        ]
    })";

    load_platform_from_string(engine, json);

    auto& cd = engine.platform().clock_domain(0);
    auto power = cd.power_at_frequency(Frequency{1.0});
    EXPECT_NEAR(power.mw, 0.1, 0.001);  // a0 term only
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(PlatformLoaderTest, InvalidJsonSyntax) {
    const char* json = "{ invalid json }";

    EXPECT_THROW(load_platform_from_string(engine, json), LoaderError);
}

TEST_F(PlatformLoaderTest, MissingRequiredFields) {
    const char* json = R"({
        "processor_types": [{"name": "cpu"}]
    })";  // Missing "performance"

    EXPECT_THROW(load_platform_from_string(engine, json), LoaderError);
}

TEST_F(PlatformLoaderTest, InvalidProcessorTypeReference) {
    const char* json = R"({
        "processor_types": [{"name": "big", "performance": 1.0}],
        "clock_domains": [{"id": 0, "frequencies_mhz": [1000]}],
        "power_domains": [{"id": 0}],
        "processors": [{"type": "nonexistent", "clock_domain": 0, "power_domain": 0}]
    })";

    EXPECT_THROW(load_platform_from_string(engine, json), LoaderError);
}

TEST_F(PlatformLoaderTest, InvalidClockDomainReference) {
    const char* json = R"({
        "processor_types": [{"name": "cpu", "performance": 1.0}],
        "clock_domains": [{"id": 0, "frequencies_mhz": [1000]}],
        "power_domains": [{"id": 0}],
        "processors": [{"type": "cpu", "clock_domain": 999, "power_domain": 0}]
    })";

    EXPECT_THROW(load_platform_from_string(engine, json), LoaderError);
}

TEST_F(PlatformLoaderTest, InvalidPowerDomainReference) {
    const char* json = R"({
        "processor_types": [{"name": "cpu", "performance": 1.0}],
        "clock_domains": [{"id": 0, "frequencies_mhz": [1000]}],
        "power_domains": [{"id": 0}],
        "processors": [{"type": "cpu", "clock_domain": 0, "power_domain": 999}]
    })";

    EXPECT_THROW(load_platform_from_string(engine, json), LoaderError);
}

TEST_F(PlatformLoaderTest, EmptyFrequenciesArray) {
    const char* json = R"({
        "processor_types": [{"name": "cpu", "performance": 1.0}],
        "clock_domains": [{"id": 0, "frequencies_mhz": []}],
        "power_domains": [{"id": 0}],
        "processors": [{"type": "cpu", "clock_domain": 0, "power_domain": 0}]
    })";

    EXPECT_THROW(load_platform_from_string(engine, json), LoaderError);
}

TEST_F(PlatformLoaderTest, RootNotObject) {
    const char* json = "[]";

    EXPECT_THROW(load_platform_from_string(engine, json), LoaderError);
}

// =============================================================================
// Empty/Minimal Tests
// =============================================================================

TEST_F(PlatformLoaderTest, EmptyPlatform) {
    const char* json = "{}";

    // Should not throw, but platform will have no resources
    load_platform_from_string(engine, json);

    auto& platform = engine.platform();
    EXPECT_EQ(platform.processor_count(), 0u);
}
