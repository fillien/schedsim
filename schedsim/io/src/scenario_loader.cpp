#include <schedsim/io/scenario_loader.hpp>
#include <schedsim/io/error.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace schedsim::io {

namespace {

using namespace schedsim::core;

// Helper to get required member with error context
template<typename T>
const T& get_member(const rapidjson::Value& obj, const char* name, const char* context) {
    if (!obj.HasMember(name)) {
        throw LoaderError(std::string("missing required field '") + name + "'", context);
    }
    return obj[name];
}

double get_double(const rapidjson::Value& val, const char* name, const char* context) {
    const auto& member = get_member<rapidjson::Value>(val, name, context);
    if (!member.IsNumber()) {
        throw LoaderError(std::string("field '") + name + "' must be a number", context);
    }
    return member.GetDouble();
}

uint64_t get_uint64(const rapidjson::Value& val, const char* name, const char* context) {
    const auto& member = get_member<rapidjson::Value>(val, name, context);
    if (!member.IsUint64()) {
        throw LoaderError(std::string("field '") + name + "' must be a non-negative integer", context);
    }
    return member.GetUint64();
}

const rapidjson::Value& get_array(const rapidjson::Value& val, const char* name, const char* context) {
    const auto& member = get_member<rapidjson::Value>(val, name, context);
    if (!member.IsArray()) {
        throw LoaderError(std::string("field '") + name + "' must be an array", context);
    }
    return member;
}

// Optional getters
double get_double_or(const rapidjson::Value& val, const char* name, double default_val) {
    if (!val.HasMember(name)) {
        return default_val;
    }
    const auto& member = val[name];
    if (!member.IsNumber()) {
        return default_val;
    }
    return member.GetDouble();
}

void parse_scenario_impl(ScenarioData& result, const rapidjson::Document& doc) {
    if (!doc.HasMember("tasks")) {
        // Empty tasks array is valid
        return;
    }

    const auto& tasks = get_array(doc, "tasks", "scenario");

    for (rapidjson::SizeType tidx = 0; tidx < tasks.Size(); ++tidx) {
        const auto& task_obj = tasks[tidx];
        std::string ctx = "tasks[" + std::to_string(tidx) + "]";

        TaskParams task_params;

        // Task ID (required)
        task_params.id = get_uint64(task_obj, "id", ctx.c_str());

        // Period (required)
        double period = get_double(task_obj, "period", ctx.c_str());
        if (period <= 0) {
            throw LoaderError("period must be positive", ctx.c_str());
        }
        task_params.period = duration_from_seconds(period);

        // Relative deadline (optional, defaults to period)
        double deadline = get_double_or(task_obj, "relative_deadline", period);
        task_params.relative_deadline = duration_from_seconds(deadline);

        // WCET: new format uses "wcet", legacy uses "utilization"
        if (task_obj.HasMember("wcet")) {
            double wcet = get_double(task_obj, "wcet", ctx.c_str());
            if (wcet <= 0) {
                throw LoaderError("wcet must be positive", ctx.c_str());
            }
            task_params.wcet = duration_from_seconds(wcet);
        } else if (task_obj.HasMember("utilization")) {
            double utilization = get_double(task_obj, "utilization", ctx.c_str());
            if (utilization <= 0 || utilization > 1.0) {
                throw LoaderError("utilization must be in (0, 1]", ctx.c_str());
            }
            task_params.wcet = duration_from_seconds(period * utilization);
        } else {
            throw LoaderError("either 'wcet' or 'utilization' must be specified", ctx.c_str());
        }

        // Validate deadline >= wcet
        if (task_params.relative_deadline < task_params.wcet) {
            throw LoaderError("relative_deadline must be >= wcet", ctx.c_str());
        }

        // Jobs (optional)
        if (task_obj.HasMember("jobs")) {
            const auto& jobs = task_obj["jobs"];
            if (jobs.IsArray()) {
                for (rapidjson::SizeType jidx = 0; jidx < jobs.Size(); ++jidx) {
                    const auto& job_obj = jobs[jidx];
                    std::string jctx = ctx + ".jobs[" + std::to_string(jidx) + "]";

                    double arrival = get_double(job_obj, "arrival", jctx.c_str());
                    double duration = get_double(job_obj, "duration", jctx.c_str());

                    if (duration <= 0) {
                        throw LoaderError("job duration must be positive", jctx.c_str());
                    }

                    task_params.jobs.push_back(JobParams{
                        time_from_seconds(arrival),
                        duration_from_seconds(duration)
                    });
                }

                // Sort jobs by arrival time
                std::sort(task_params.jobs.begin(), task_params.jobs.end(),
                    [](const JobParams& lhs, const JobParams& rhs) {
                        return lhs.arrival < rhs.arrival;
                    });
            }
        }

        result.tasks.push_back(std::move(task_params));
    }
}

} // anonymous namespace

ScenarioData load_scenario(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw LoaderError("cannot open file", path.string());
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return load_scenario_from_string(oss.str());
}

ScenarioData load_scenario_from_string(std::string_view json) {
    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());

    if (doc.HasParseError()) {
        throw LoaderError(
            std::string("JSON parse error: ") + rapidjson::GetParseError_En(doc.GetParseError()),
            "at offset " + std::to_string(doc.GetErrorOffset()));
    }

    if (!doc.IsObject()) {
        throw LoaderError("root must be an object", "scenario");
    }

    ScenarioData result;
    parse_scenario_impl(result, doc);
    return result;
}

void write_scenario_to_stream(const ScenarioData& scenario, std::ostream& out) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.StartObject();
    writer.Key("tasks");
    writer.StartArray();

    for (const auto& task : scenario.tasks) {
        writer.StartObject();

        writer.Key("id");
        writer.Uint64(task.id);

        writer.Key("period");
        writer.Double(duration_to_seconds(task.period));

        writer.Key("relative_deadline");
        writer.Double(duration_to_seconds(task.relative_deadline));

        writer.Key("wcet");
        writer.Double(duration_to_seconds(task.wcet));

        if (!task.jobs.empty()) {
            writer.Key("jobs");
            writer.StartArray();
            for (const auto& job : task.jobs) {
                writer.StartObject();
                writer.Key("arrival");
                writer.Double(time_to_seconds(job.arrival));
                writer.Key("duration");
                writer.Double(duration_to_seconds(job.duration));
                writer.EndObject();
            }
            writer.EndArray();
        }

        writer.EndObject();
    }

    writer.EndArray();
    writer.EndObject();

    out << buffer.GetString();
}

void write_scenario(const ScenarioData& scenario, const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file) {
        throw LoaderError("cannot open file for writing", path.string());
    }
    write_scenario_to_stream(scenario, file);
}

} // namespace schedsim::io
