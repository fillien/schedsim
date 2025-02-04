#include "rapidjson/rapidjson.h"
#include <filesystem>
#include <fstream>
#include <protocols/hardware.hpp>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <vector>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace protocols::hardware {

auto to_json(const cluster& plat, rapidjson::Document::AllocatorType& allocator) -> rapidjson::Value
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        rapidjson::Value res(rapidjson::kObjectType);

        res.AddMember("procs", plat.nb_procs, allocator);

        rapidjson::Value freq_array(rapidjson::kArrayType);
        for (const auto& freq : plat.frequencies) {
                freq_array.PushBack(freq, allocator);
        }
        res.AddMember("frequencies", freq_array, allocator);
        res.AddMember("effective_freq", plat.effective_freq, allocator);

        rapidjson::Value power_model_array(rapidjson::kArrayType);
        for (const auto& power : plat.power_model) {
                power_model_array.PushBack(power, allocator);
        }
        res.AddMember("power_model", power_model_array, allocator);
        res.AddMember("perf_score", plat.perf_score, allocator);

        return res;
}

void to_json(const hardware& plat, rapidjson::Document& doc)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        rapidjson::Value cluster_array(rapidjson::kArrayType);
        for (const auto& clu : plat.clusters) {
                cluster_array.PushBack(to_json(clu, allocator), allocator);
        }
        doc.AddMember("clusters", cluster_array, allocator);
}

auto from_json_cluster(const rapidjson::Value& value) -> cluster
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        cluster clu;

        if (!value.HasMember("procs") || !value["procs"].IsUint64()) {
                throw std::runtime_error("Invalid or missing 'procs' field");
        }
        clu.nb_procs = value["procs"].GetUint64();

        if (!value.HasMember("frequencies") || !value["frequencies"].IsArray()) {
                throw std::runtime_error("Invalid or missing 'frequencies' field");
        }
        const auto& freq_array = value["frequencies"].GetArray();
        clu.frequencies.reserve(freq_array.Size());
        for (const auto& freq : freq_array) {
                if (!freq.IsDouble()) { throw std::runtime_error("Invalid frequency value"); }
                clu.frequencies.push_back(freq.GetDouble());
        }

        if (!value.HasMember("effective_freq") || !value["effective_freq"].IsDouble()) {
                throw std::runtime_error("Invalid or missing 'effective_freq' field");
        }
        clu.effective_freq = value["effective_freq"].GetDouble();

        if (!value.HasMember("power_model") || !value["power_model"].IsArray()) {
                throw std::runtime_error("Invalid or missing 'power_model' field");
        }
        const auto& power_model_array = value["power_model"].GetArray();
        clu.power_model.reserve(power_model_array.Size());
        for (const auto& power : power_model_array) {
                if (!power.IsDouble()) { throw std::runtime_error("Invalid power model value"); }
                clu.power_model.push_back(power.GetDouble());
        }

        if (!value.HasMember("perf_score") || !value["perf_score"].IsDouble()) {
                throw std::runtime_error("Invalid or missing 'perf_score' field");
        }
        clu.perf_score = value["perf_score"].GetDouble();

        return clu;
}

auto from_json_hardware(const rapidjson::Document& doc) -> hardware
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif

        if (!doc.HasMember("clusters") || !doc["clusters"].IsArray()) {
                throw std::runtime_error("Invalid or missing 'clusters' field");
        }

        hardware plat;
        const auto& clusters_array = doc["clusters"].GetArray();
        plat.clusters.reserve(clusters_array.Size());

        for (const auto& clu : clusters_array) {
                plat.clusters.push_back(from_json_cluster(clu));
        }

        return plat;
}

void write_file(const std::filesystem::path& file, const hardware& plat)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        rapidjson::Document doc;
        to_json(plat, doc);

        std::ofstream ofs(file);
        if (!ofs) { throw std::runtime_error("Unable to open file: " + file.string()); }

        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        doc.Accept(writer);
}

auto read_file(const std::filesystem::path& file) -> hardware
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

        return from_json_hardware(doc);
}
} // namespace protocols::hardware
