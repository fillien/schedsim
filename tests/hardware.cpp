#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <protocols/hardware.hpp>

using namespace protocols::hardware;

class HardwareTest : public ::testing::Test {};

TEST_F(HardwareTest, ConvertToJsonTest)
{
        hardware original{5, {1.3, 2.5, 3.2}};

        auto json = to_json(original);
        auto converted = from_json_hardware(json);
        EXPECT_EQ(converted.nb_procs, original.nb_procs);
        EXPECT_THAT(converted.frequencies, ::testing::ElementsAre(1.3, 2.5, 3.2));
}

TEST_F(HardwareTest, FileWriteRead)
{
        hardware original{5, {1.3, 2.5, 3.2}};
        hardware converted;

        std::filesystem::path temp_file =
            std::filesystem::temp_directory_path() / "hardware_test.json";
        EXPECT_NO_THROW(write_file(temp_file, original));
        EXPECT_NO_THROW(converted = read_file(temp_file));

        EXPECT_EQ(converted.nb_procs, original.nb_procs);
        EXPECT_THAT(converted.frequencies, ::testing::ElementsAre(1.3, 2.5, 3.2));

        // Clean up: remove the temporary file
        if (std::filesystem::exists(temp_file)) { std::filesystem::remove(temp_file); }
}
