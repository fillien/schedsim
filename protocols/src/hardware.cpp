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

void to_json(const hardware& plat, rapidjson::Document& doc)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        doc.AddMember("procs", plat.nb_procs, allocator);

        rapidjson::Value freq_array(rapidjson::kArrayType);
        for (const auto& freq : plat.frequencies) {
                freq_array.PushBack(freq, allocator);
        }
        doc.AddMember("frequencies", freq_array, allocator);
        doc.AddMember("effective_freq", plat.effective_freq, allocator);

        rapidjson::Value power_model_array(rapidjson::kArrayType);
        for (const auto& power : plat.power_model) {
                power_model_array.PushBack(power, allocator);
        }
        doc.AddMember("power_model", power_model_array, allocator);
}

auto from_json_hardware(const rapidjson::Document& doc) -> hardware
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        hardware plat;

        plat.nb_procs = doc["procs"].GetUint64();

        const auto& freq_array = doc["frequencies"].GetArray();
        plat.frequencies.reserve(freq_array.Size());
        for (const auto& freq : freq_array) {
                plat.frequencies.push_back(freq.GetDouble());
        }

        plat.effective_freq = doc["effective_freq"].GetDouble();

        const auto& power_model_array = doc["power_model"].GetArray();
        plat.power_model.reserve(power_model_array.Size());
        for (const auto& power : power_model_array) {
                plat.power_model.push_back(power.GetDouble());
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
