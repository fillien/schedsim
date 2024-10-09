#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <protocols/hardware.hpp>

using namespace protocols::hardware;

class HardwareTest : public ::testing::Test {};

const hardware original{
    5,
    {1.3, 2.5, 3.2},
    1.3,
    {0.04433100178, 0.000003410453667, 0.00000002193142733, 0.00000000004609381282}};

TEST_F(HardwareTest, ConvertToJsonTest)
{
        auto json = to_json(original);
        auto converted = from_json_hardware(json);
        EXPECT_EQ(converted.nb_procs, original.nb_procs);
        EXPECT_THAT(converted.frequencies, ::testing::ElementsAre(1.3, 2.5, 3.2));
        EXPECT_EQ(converted.effective_freq, original.effective_freq);
        EXPECT_EQ(converted.power_model, original.power_model);
}

TEST_F(HardwareTest, FileWriteRead)
{
        namespace fs = std::filesystem;
        hardware converted;

        fs::path temp_file = fs::temp_directory_path() / "hardware_test.json";
        EXPECT_NO_THROW(write_file(temp_file, original));
        EXPECT_NO_THROW(converted = read_file(temp_file));

        EXPECT_EQ(converted.nb_procs, original.nb_procs);
        EXPECT_THAT(converted.frequencies, ::testing::ElementsAre(1.3, 2.5, 3.2));
        EXPECT_EQ(converted.effective_freq, original.effective_freq);
        EXPECT_EQ(converted.power_model, original.power_model);

        // Clean up: remove the temporary file
        if (fs::exists(temp_file)) { fs::remove(temp_file); }
}
