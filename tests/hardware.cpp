#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <protocols/hardware.hpp>
#include <vector>

using namespace protocols::hardware;

class HardwareTest : public ::testing::Test {};

const hardware original{
    {cluster{
         5,
         {1.3, 2.5, 3.2},
         1.3,
         {0.04433100178, 0.000003410453667, 0.00000002193142733, 0.00000000004609381282}},
     cluster{
         2,
         {1.3, 2.5, 3.2},
         1.3,
         {0.04433100178, 0.000003410453667, 0.00000002193142733, 0.00000000004609381282}}}};

TEST_F(HardwareTest, ConvertToJsonTest)
{
        rapidjson::Document json_doc;
        to_json(original, json_doc);
        auto orig_clu = original.clusters.at(0);
        auto converted = from_json_hardware(json_doc).clusters.at(0);
        EXPECT_EQ(converted.nb_procs, orig_clu.nb_procs);
        EXPECT_THAT(converted.frequencies, ::testing::ElementsAre(1.3, 2.5, 3.2));
        EXPECT_EQ(converted.effective_freq, orig_clu.effective_freq);
        EXPECT_EQ(converted.power_model, orig_clu.power_model);
}

TEST_F(HardwareTest, FileWriteRead)
{
        namespace fs = std::filesystem;
        cluster converted;
        auto orig_clu = original.clusters.at(0);
        fs::path temp_file = fs::temp_directory_path() / "hardware_test.json";
        EXPECT_NO_THROW(write_file(temp_file, original));
        EXPECT_NO_THROW(converted = read_file(temp_file).clusters.at(0));
        EXPECT_EQ(converted.nb_procs, orig_clu.nb_procs);
        EXPECT_THAT(converted.frequencies, ::testing::ElementsAre(1.3, 2.5, 3.2));
        EXPECT_EQ(converted.effective_freq, orig_clu.effective_freq);
        EXPECT_EQ(converted.power_model, orig_clu.power_model);

        if (fs::exists(temp_file)) { fs::remove(temp_file); }
}
