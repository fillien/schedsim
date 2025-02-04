#ifndef HARDWARE_HPP
#define HARDWARE_HPP

#include <cstddef>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <vector>

namespace protocols::hardware {

struct cluster {
        uint64_t nb_procs;
        std::vector<double> frequencies;
        double effective_freq;
        std::vector<double> power_model;
        double perf_score;
};

struct hardware {
        std::vector<cluster> clusters;
};

void to_json(const hardware& plat, rapidjson::Document& doc);
auto from_json_hardware(const rapidjson::Document& doc) -> hardware;
void write_file(const std::filesystem::path& file, const hardware& plat);
auto read_file(const std::filesystem::path& file) -> hardware;
} // namespace protocols::hardware

#endif
