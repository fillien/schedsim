#ifndef HARDWARE_HPP
#define HARDWARE_HPP

#include <cstddef>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <vector>

namespace protocols::hardware {

struct Cluster {
        uint64_t nb_procs;
        std::vector<double> frequencies;
        double effective_freq;
        std::vector<double> power_model;
        double perf_score;
        double u_target;
};

struct Hardware {
        std::vector<Cluster> clusters;
};

void to_json(const Hardware& plat, rapidjson::Document& doc);
auto from_json_hardware(const rapidjson::Document& doc) -> Hardware;
void write_file(const std::filesystem::path& file, const Hardware& plat);
auto read_file(const std::filesystem::path& file) -> Hardware;
} // namespace protocols::hardware

#endif
