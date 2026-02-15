#include <schedsim/io/metrics.hpp>
#include <schedsim/io/error.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace schedsim::io {

namespace {

// Helper to extract uint64_t from variant field
uint64_t get_uint64_field(const TraceRecord& record, const std::string& key, uint64_t default_val = 0) {
    auto iter = record.fields.find(key);
    if (iter == record.fields.end()) {
        return default_val;
    }
    if (std::holds_alternative<uint64_t>(iter->second)) {
        return std::get<uint64_t>(iter->second);
    }
    if (std::holds_alternative<double>(iter->second)) {
        return static_cast<uint64_t>(std::get<double>(iter->second));
    }
    return default_val;
}

double get_double_field(const TraceRecord& record, const std::string& key, double default_val = 0.0) {
    auto iter = record.fields.find(key);
    if (iter == record.fields.end()) {
        return default_val;
    }
    if (std::holds_alternative<double>(iter->second)) {
        return std::get<double>(iter->second);
    }
    if (std::holds_alternative<uint64_t>(iter->second)) {
        return static_cast<double>(std::get<uint64_t>(iter->second));
    }
    return default_val;
}

} // anonymous namespace

SimulationMetrics compute_metrics(const std::vector<TraceRecord>& traces) {
    SimulationMetrics metrics;

    // Track job arrivals for response time computation
    // Map of (task_id, job_id) -> arrival_time
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, double>> arrival_times;

    // Track processor active time for utilization
    std::unordered_map<uint64_t, double> active_time;
    double simulation_end = 0.0;

    for (const auto& record : traces) {
        simulation_end = std::max(simulation_end, record.time);

        if (record.type == "job_arrival") {
            metrics.total_jobs++;
            uint64_t tid = get_uint64_field(record, "task_id");
            uint64_t jid = get_uint64_field(record, "job_id");
            arrival_times[tid][jid] = record.time;
        }
        else if (record.type == "job_completion") {
            metrics.completed_jobs++;
            uint64_t tid = get_uint64_field(record, "task_id");
            uint64_t jid = get_uint64_field(record, "job_id");

            // Compute response time
            auto task_it = arrival_times.find(tid);
            if (task_it != arrival_times.end()) {
                auto job_it = task_it->second.find(jid);
                if (job_it != task_it->second.end()) {
                    double response_time = record.time - job_it->second;
                    metrics.response_times_per_task[tid].push_back(response_time);
                }
            }
        }
        else if (record.type == "deadline_miss") {
            metrics.deadline_misses++;
            uint64_t tid = get_uint64_field(record, "task_id");
            metrics.deadline_misses_per_task[tid]++;
        }
        else if (record.type == "task_rejected") {
            metrics.rejected_tasks++;
        }
        else if (record.type == "frequency_change") {
            SimulationMetrics::FrequencyChange fc;
            fc.time = record.time;
            fc.clock_domain_id = get_uint64_field(record, "clock_domain_id");
            fc.old_freq_mhz = get_double_field(record, "old_freq_mhz");
            fc.new_freq_mhz = get_double_field(record, "new_freq_mhz");
            metrics.frequency_changes.push_back(fc);
        }
        else if (record.type == "job_start") {
            uint64_t tid = get_uint64_field(record, "task_id");
            auto task_it = arrival_times.find(tid);
            if (task_it != arrival_times.end()) {
                uint64_t jid = get_uint64_field(record, "job_id");
                auto job_it = task_it->second.find(jid);
                if (job_it != task_it->second.end()) {
                    double wait = record.time - job_it->second;
                    metrics.waiting_times_per_task[tid].push_back(wait);
                }
            }
        }
        else if (record.type == "preemption") {
            metrics.preemptions++;
        }
        else if (record.type == "context_switch") {
            metrics.context_switches++;
        }
        else if (record.type == "energy") {
            uint64_t proc = get_uint64_field(record, "proc");
            double energy = get_double_field(record, "energy_mj");
            metrics.energy_per_processor[proc] += energy;
            metrics.total_energy_mj += energy;
        }
        else if (record.type == "processor_active") {
            uint64_t proc = get_uint64_field(record, "proc");
            double duration = get_double_field(record, "duration");
            active_time[proc] += duration;
        }
    }

    // Compute utilization
    if (simulation_end > 0.0) {
        double total_active = 0.0;
        for (const auto& [proc, active] : active_time) {
            metrics.utilization_per_processor[proc] = active / simulation_end;
            total_active += active;
        }
        if (!active_time.empty()) {
            metrics.average_utilization = total_active / (simulation_end * active_time.size());
        }
    }

    return metrics;
}

SimulationMetrics compute_metrics_from_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw LoaderError("cannot open file", path.string());
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    std::string json = oss.str();

    rapidjson::Document doc;
    doc.Parse(json.c_str());

    if (doc.HasParseError()) {
        throw LoaderError(
            std::string("JSON parse error: ") + rapidjson::GetParseError_En(doc.GetParseError()),
            "at offset " + std::to_string(doc.GetErrorOffset()));
    }

    if (!doc.IsArray()) {
        throw LoaderError("trace file must be a JSON array", path.string());
    }

    std::vector<TraceRecord> traces;
    traces.reserve(doc.Size());

    for (rapidjson::SizeType idx = 0; idx < doc.Size(); ++idx) {
        const auto& obj = doc[idx];
        TraceRecord record;

        if (obj.HasMember("time") && obj["time"].IsNumber()) {
            record.time = obj["time"].GetDouble();
        }
        if (obj.HasMember("type") && obj["type"].IsString()) {
            record.type = obj["type"].GetString();
        }

        // Extract all other fields
        for (auto iter = obj.MemberBegin(); iter != obj.MemberEnd(); ++iter) {
            std::string key = iter->name.GetString();
            if (key == "time" || key == "type") {
                continue;
            }
            if (iter->value.IsDouble()) {
                record.fields[key] = iter->value.GetDouble();
            } else if (iter->value.IsUint64()) {
                record.fields[key] = iter->value.GetUint64();
            } else if (iter->value.IsInt64()) {
                record.fields[key] = static_cast<uint64_t>(iter->value.GetInt64());
            } else if (iter->value.IsInt()) {
                record.fields[key] = static_cast<uint64_t>(iter->value.GetInt());
            } else if (iter->value.IsString()) {
                record.fields[key] = std::string(iter->value.GetString());
            }
        }

        traces.push_back(std::move(record));
    }

    return compute_metrics(traces);
}

ResponseTimeStats compute_response_time_stats(const std::vector<double>& response_times) {
    ResponseTimeStats stats;

    if (response_times.empty()) {
        return stats;
    }

    // Make a sorted copy for percentile calculations
    std::vector<double> sorted = response_times;
    std::sort(sorted.begin(), sorted.end());

    // Min and max
    stats.min = sorted.front();
    stats.max = sorted.back();

    // Mean
    double sum = 0.0;
    for (double val : sorted) {
        sum += val;
    }
    stats.mean = sum / static_cast<double>(sorted.size());

    // Median
    std::size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0) {
        stats.median = (sorted[mid - 1] + sorted[mid]) / 2.0;
    } else {
        stats.median = sorted[mid];
    }

    // Standard deviation
    double variance_sum = 0.0;
    for (double val : sorted) {
        double diff = val - stats.mean;
        variance_sum += diff * diff;
    }
    stats.stddev = std::sqrt(variance_sum / static_cast<double>(sorted.size()));

    // Percentiles using linear interpolation
    auto percentile = [&sorted](double pct) -> double {
        if (sorted.size() == 1) {
            return sorted[0];
        }
        double rank = pct / 100.0 * static_cast<double>(sorted.size() - 1);
        auto lower = static_cast<std::size_t>(std::floor(rank));
        auto upper = static_cast<std::size_t>(std::ceil(rank));
        if (lower == upper) {
            return sorted[lower];
        }
        double frac = rank - static_cast<double>(lower);
        return sorted[lower] * (1.0 - frac) + sorted[upper] * frac;
    };

    stats.percentile_95 = percentile(95.0);
    stats.percentile_99 = percentile(99.0);

    return stats;
}

} // namespace schedsim::io
