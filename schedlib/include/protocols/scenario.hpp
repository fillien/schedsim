#ifndef SCENARIO_HPP
#define SCENARIO_HPP

#include <filesystem>
#include <rapidjson/document.h>
#include <vector>

namespace protocols::scenario {

/**
 * @brief Represents a job within a task.
 */
struct Job {
        /**
         * @brief The arrival time of the job.
         */
        double arrival;
        /**
         * @brief The duration of the job.
         */
        double duration;
};

/**
 * @brief Represents a task with its properties and associated jobs.
 */
struct Task {
        /**
         * @brief Unique identifier for the task.
         */
        uint64_t id;
        /**
         * @brief Utilization factor of the task.  Represents the proportion of time the task is
         * active.
         */
        double utilization;
        /**
         * @brief The period of the task, defining how often it repeats.
         */
        double period;
        /**
         * @brief A vector containing all jobs associated with this task.
         */
        std::vector<Job> jobs;
};

/**
 * @brief Represents a collection of tasks and their settings.
 */
struct Setting {
        /**
         * @brief A vector containing all tasks within the setting.
         */
        std::vector<Task> tasks;
};

/**
 * @brief Converts a Job object to a rapidjson Value.
 *
 * @param job The Job object to convert.
 * @param allocator The rapidjson document allocator.
 * @param job_json A reference to the rapidjson Value that will store the serialized Job data.
 */
void to_json(
    const Job& job, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& job_json);

/**
 * @brief Converts a Task object to a rapidjson Value.
 *
 * @param task The Task object to convert.
 * @param allocator The rapidjson document allocator.
 * @param task_json A reference to the rapidjson Value that will store the serialized Task data.
 */
void to_json(
    const Task& task, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& task_json);

/**
 * @brief Converts a Setting object to a rapidjson Value.
 *
 * @param setting The Setting object to convert.
 * @param allocator The rapidjson document allocator.
 * @param setting_json A reference to the rapidjson Value that will store the serialized Setting
 * data.
 */
void to_json(
    const Setting& setting,
    rapidjson::Document::AllocatorType& allocator,
    rapidjson::Value& setting_json);

/**
 * @brief Creates a Job object from a rapidjson Value.
 *
 * @param json_job The rapidjson Value containing the serialized Job data.
 * @return A Job object created from the JSON data.
 */
auto from_json_job(const rapidjson::Value& json_job) -> Job;

/**
 * @brief Creates a Task object from a rapidjson Value.
 *
 * @param json_task The rapidjson Value containing the serialized Task data.
 * @return A Task object created from the JSON data.
 */
auto from_json_task(const rapidjson::Value& json_task) -> Task;

/**
 * @brief Creates a Setting object from a rapidjson Value.
 *
 * @param json_setting The rapidjson Value containing the serialized Setting data.
 * @return A Setting object created from the JSON data.
 */
auto from_json_setting(const rapidjson::Value& json_setting) -> Setting;

/**
 * @brief Writes a Setting object to a file in JSON format.
 *
 * @param file The path to the file where the Setting will be written.
 * @param tasks The Setting object to write.
 */
void write_file(const std::filesystem::path& file, const Setting& tasks);

/**
 * @brief Reads a Setting object from a file in JSON format.
 *
 * @param file The path to the file where the Setting is stored.
 * @return A Setting object read from the JSON data in the file.
 */
auto read_file(const std::filesystem::path& file) -> Setting;

} // namespace protocols::scenario

#endif