#include <filesystem>
#include <fstream>
#include <protocols/scenario.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace protocols::scenario {
void to_json(
    const Job& job, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& job_json)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        job_json.SetObject();
        job_json.AddMember("arrival", job.arrival, allocator);
        job_json.AddMember("duration", job.duration, allocator);
}

void to_json(
    const Task& task, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& task_json)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        task_json.SetObject();
        task_json.AddMember("id", task.id, allocator);
        task_json.AddMember("period", task.period, allocator);
        task_json.AddMember("utilization", task.utilization, allocator);

        rapidjson::Value jobs_json(rapidjson::kArrayType);
        for (const auto& job : task.jobs) {
                rapidjson::Value job_json(rapidjson::kObjectType);
                to_json(job, allocator, job_json);
                jobs_json.PushBack(job_json, allocator);
        }
        task_json.AddMember("jobs", jobs_json, allocator);
}

void to_json(
    const Setting& setting,
    rapidjson::Document::AllocatorType& allocator,
    rapidjson::Value& setting_json)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        setting_json.SetObject();
        rapidjson::Value tasks_json(rapidjson::kArrayType);
        for (const auto& task : setting.tasks) {
                rapidjson::Value task_json(rapidjson::kObjectType);
                to_json(task, allocator, task_json);
                tasks_json.PushBack(task_json, allocator);
        }
        setting_json.AddMember("tasks", tasks_json, allocator);
}

auto from_json_job(const rapidjson::Value& json_job) -> Job
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        if (!json_job.HasMember("arrival") || !json_job["arrival"].IsDouble()) {
                throw std::runtime_error("Invalid or missing 'arrival' field");
        }
        if (!json_job.HasMember("duration") || !json_job["duration"].IsDouble()) {
                throw std::runtime_error("Invalid or missing 'duration' field");
        }
        return Job{
            .arrival = json_job["arrival"].GetDouble(),
            .duration = json_job["duration"].GetDouble()};
}

auto from_json_task(const rapidjson::Value& json_task) -> Task
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (!json_task.HasMember("id") || !json_task["id"].IsUint64()) {
                throw std::runtime_error("Invalid or missing 'id' field");
        }
        if (!json_task.HasMember("utilization") || !json_task["utilization"].IsDouble()) {
                throw std::runtime_error("Invalid or missing 'utilization' field");
        }
        if (!json_task.HasMember("period") || !json_task["period"].IsDouble()) {
                throw std::runtime_error("Invalid or missing 'period' field");
        }
        if (!json_task.HasMember("jobs") || !json_task["jobs"].IsArray()) {
                throw std::runtime_error("Invalid or missing 'jobs' field");
        }
        Task parsed_task{
            .id = json_task["id"].GetUint64(),
            .utilization = json_task["utilization"].GetDouble(),
            .period = json_task["period"].GetDouble(),
            .jobs = {}};

        const rapidjson::Value& jobs_json = json_task["jobs"];
        for (rapidjson::SizeType i = 0; i < jobs_json.Size(); ++i) {
                parsed_task.jobs.push_back(from_json_job(jobs_json[i]));
        }

        return parsed_task;
}

auto from_json_setting(const rapidjson::Value& json_setting) -> Setting
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (!json_setting.HasMember("tasks") || !json_setting["tasks"].IsArray()) {
                throw std::runtime_error("Invalid or missing 'tasks' field");
        }

        std::vector<Task> tasks;
        const rapidjson::Value& tasks_json = json_setting["tasks"];
        for (rapidjson::SizeType i = 0; i < tasks_json.Size(); ++i) {
                tasks.push_back(from_json_task(tasks_json[i]));
        }
        return Setting{tasks};
}

void write_file(const std::filesystem::path& file, const Setting& setting)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::ofstream ofs(file);
        if (!ofs) { throw std::runtime_error("Unable to open file: " + file.string()); }

        rapidjson::Document doc;
        doc.SetObject();
        to_json(setting, doc.GetAllocator(), doc);

        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        doc.Accept(writer);
}

auto read_file(const std::filesystem::path& file) -> Setting
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::ifstream ifs(file);
        if (!ifs) { throw std::runtime_error("Failed to open file: " + file.string()); }

        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document doc;
        doc.ParseStream(isw);
        if (doc.HasParseError()) {
                throw std::runtime_error("JSON parsing error in file " + file.string());
        }

        return from_json_setting(doc);
}
} // namespace protocols::scenario
