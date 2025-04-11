#ifndef HARDWARE_HPP
#define HARDWARE_HPP

#include <cstddef>
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <vector>

namespace protocols::hardware {

/**
 * @brief Represents a cluster of processing units within a hardware platform.
 */
struct Cluster {
        /**
         * @brief The number of processors in the cluster.
         */
        uint64_t nb_procs;
        /**
         * @brief A vector representing the frequencies available for this cluster.
         */
        std::vector<double> frequencies;
        /**
         * @brief The effective frequency of the cluster.
         */
        double effective_freq;
        /**
         * @brief A vector representing the power model for this cluster.
         */
        std::vector<double> power_model;
        /**
         * @brief The performance score of the cluster.
         */
        double perf_score;
};

/**
 * @brief Represents a hardware platform with multiple clusters.
 */
struct Hardware {
        /**
         * @brief A vector of Cluster objects representing the different clusters in the hardware
         * platform.
         */
        std::vector<Cluster> clusters;
};

/**
 * @brief Converts a Hardware object to a rapidjson Document.
 *
 * @param plat The Hardware object to convert.
 * @param doc  The rapidjson Document to populate with the Hardware data.
 */
void to_json(const Hardware& plat, rapidjson::Document& doc);

/**
 * @brief Creates a Hardware object from a rapidjson Document.
 *
 * @param doc The rapidjson Document containing the Hardware data.
 * @return A Hardware object populated with the data from the document.
 */
auto from_json_hardware(const rapidjson::Document& doc) -> Hardware;

/**
 * @brief Writes a Hardware object to a file in JSON format.
 *
 * @param file The path to the file to write to.
 * @param plat The Hardware object to write.
 */
void write_file(const std::filesystem::path& file, const Hardware& plat);

/**
 * @brief Reads a Hardware object from a file in JSON format.
 *
 * @param file The path to the file to read from.
 * @return A Hardware object populated with the data from the file.
 */
auto read_file(const std::filesystem::path& file) -> Hardware;

} // namespace protocols::hardware

#endif