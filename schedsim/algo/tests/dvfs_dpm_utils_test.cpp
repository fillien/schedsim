#include <schedsim/algo/dvfs_dpm_utils.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/processor.hpp>

#include <gtest/gtest.h>

using namespace schedsim::algo::dvfs_dpm;
using namespace schedsim::core;

// =============================================================================
// compute_freq_min Tests
// =============================================================================

TEST(DvfsDpmUtilsTest, ComputeFreqMin_Basic) {
    // f_min = f_max * (total_util + (m-1)*max_util) / m
    // = 2000 * (1.2 + 3*0.5) / 4 = 2000 * 2.7 / 4 = 1350.0
    double result = compute_freq_min(2000.0, 1.2, 0.5, 4.0);
    EXPECT_DOUBLE_EQ(result, 1350.0);
}

TEST(DvfsDpmUtilsTest, ComputeFreqMin_SingleCore) {
    // f_min = 1000 * (0.8 + 0*0.8) / 1 = 800.0
    double result = compute_freq_min(1000.0, 0.8, 0.8, 1.0);
    EXPECT_DOUBLE_EQ(result, 800.0);
}

// =============================================================================
// clamp_procs Tests
// =============================================================================

TEST(DvfsDpmUtilsTest, ClampProcs_Range) {
    // Below min → 1
    EXPECT_EQ(clamp_procs(0.5, 4), 1u);
    // In range, ceil
    EXPECT_EQ(clamp_procs(2.3, 4), 3u);
    // Above max → max
    EXPECT_EQ(clamp_procs(6.0, 4), 4u);
}

// =============================================================================
// compute_utilization_scale Tests
// =============================================================================

TEST(DvfsDpmUtilsTest, ComputeUtilizationScale_Valid) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("big", 2.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
    });
    engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    // ref_freq_max = 2000 (only one clock domain)
    // domain_freq_max = 2000, domain_perf = 2.0
    // scale = 2000 / (2000 * 2.0) = 0.5
    double scale = compute_utilization_scale(engine.platform(), cd);
    EXPECT_DOUBLE_EQ(scale, 0.5);
}

// =============================================================================
// count_active_processors Tests
// =============================================================================

TEST(DvfsDpmUtilsTest, CountActiveProcessors_MixedStates) {
    Engine engine;
    auto& pt = engine.platform().add_processor_type("big", 1.0);
    auto& cd = engine.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
    auto& pd = engine.platform().add_power_domain({
        {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}},
        {1, CStateScope::PerProcessor, duration_from_seconds(0.01), Power{50.0}}
    });
    auto& proc0 = engine.platform().add_processor(pt, cd, pd);
    auto& proc1 = engine.platform().add_processor(pt, cd, pd);
    auto& proc2 = engine.platform().add_processor(pt, cd, pd);
    engine.platform().finalize();

    // Put one processor to sleep
    proc2.request_cstate(1);

    Processor* procs[] = {&proc0, &proc1, &proc2};
    std::span<Processor* const> span(procs, 3);
    EXPECT_EQ(count_active_processors(span), 2u);
}
