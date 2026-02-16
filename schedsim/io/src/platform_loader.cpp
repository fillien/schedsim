#include <schedsim/io/platform_loader.hpp>
#include <schedsim/io/error.hpp>

#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor_type.hpp>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

int get_int(const rapidjson::Value& val, const char* name, const char* context) {
    const auto& member = get_member<rapidjson::Value>(val, name, context);
    if (!member.IsInt()) {
        throw LoaderError(std::string("field '") + name + "' must be an integer", context);
    }
    return member.GetInt();
}

uint64_t get_uint64(const rapidjson::Value& val, const char* name, const char* context) {
    const auto& member = get_member<rapidjson::Value>(val, name, context);
    if (!member.IsUint64()) {
        throw LoaderError(std::string("field '") + name + "' must be a non-negative integer", context);
    }
    return member.GetUint64();
}

std::string get_string(const rapidjson::Value& val, const char* name, const char* context) {
    const auto& member = get_member<rapidjson::Value>(val, name, context);
    if (!member.IsString()) {
        throw LoaderError(std::string("field '") + name + "' must be a string", context);
    }
    return member.GetString();
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

// Load new schema format
void load_new_format(Engine& engine, const rapidjson::Document& doc) {
    auto& platform = engine.platform();

    // Map to track created domains by ID
    std::unordered_map<uint64_t, ClockDomain*> clock_domains;
    std::unordered_map<uint64_t, PowerDomain*> power_domains;
    std::unordered_map<std::string, ProcessorType*> processor_types;

    // Load processor types
    if (doc.HasMember("processor_types")) {
        const auto& types = get_array(doc, "processor_types", "platform");
        for (rapidjson::SizeType idx = 0; idx < types.Size(); ++idx) {
            const auto& type_obj = types[idx];
            std::string ctx = "processor_types[" + std::to_string(idx) + "]";

            std::string name = get_string(type_obj, "name", ctx.c_str());
            double performance = get_double(type_obj, "performance", ctx.c_str());
            double cs_delay = get_double_or(type_obj, "context_switch_delay_us", 0.0);

            auto& proc_type = platform.add_processor_type(
                name, performance, duration_from_seconds(cs_delay / 1e6));  // Convert us to seconds
            processor_types[name] = &proc_type;
        }
    }

    // Load clock domains
    if (doc.HasMember("clock_domains")) {
        const auto& domains = get_array(doc, "clock_domains", "platform");
        for (rapidjson::SizeType idx = 0; idx < domains.Size(); ++idx) {
            const auto& cd_obj = domains[idx];
            std::string ctx = "clock_domains[" + std::to_string(idx) + "]";

            auto cd_id = get_uint64(cd_obj, "id", ctx.c_str());

            // Get frequencies array
            const auto& freqs = get_array(cd_obj, "frequencies_mhz", ctx.c_str());
            if (freqs.Empty()) {
                throw LoaderError("frequencies_mhz array cannot be empty", ctx.c_str());
            }

            // Frequencies should be sorted ascending - extract min and max
            double freq_min = freqs[0].GetDouble();
            double freq_max = freqs[freqs.Size() - 1].GetDouble();

            double transition_delay = get_double_or(cd_obj, "transition_delay_us", 0.0);

            auto& cd = platform.add_clock_domain(
                Frequency{freq_min},
                Frequency{freq_max},
                duration_from_seconds(transition_delay / 1e6));  // Convert us to seconds

            // Set discrete frequency modes if multiple frequencies
            if (freqs.Size() > 1) {
                std::vector<Frequency> modes;
                modes.reserve(freqs.Size());
                for (rapidjson::SizeType fi = 0; fi < freqs.Size(); ++fi) {
                    modes.push_back(Frequency{freqs[fi].GetDouble()});
                }
                cd.set_frequency_modes(std::move(modes));
            }

            // Set effective frequency if specified
            if (cd_obj.HasMember("effective_frequency_mhz")) {
                cd.set_freq_eff(Frequency{cd_obj["effective_frequency_mhz"].GetDouble()});
            }

            // Set initial frequency if specified
            if (cd_obj.HasMember("initial_frequency_mhz")) {
                double initial_freq = cd_obj["initial_frequency_mhz"].GetDouble();
                cd.set_frequency(Frequency{initial_freq});
            }

            // Set power model if specified
            if (cd_obj.HasMember("power_model")) {
                const auto& pm = cd_obj["power_model"];
                if (pm.IsArray() && pm.Size() == 4) {
                    std::array<double, 4> coeffs = {
                        pm[0].GetDouble(),
                        pm[1].GetDouble(),
                        pm[2].GetDouble(),
                        pm[3].GetDouble()
                    };
                    cd.set_power_coefficients(coeffs);
                }
            }

            clock_domains[cd_id] = &cd;
        }
    }

    // Load power domains
    if (doc.HasMember("power_domains")) {
        const auto& domains = get_array(doc, "power_domains", "platform");
        for (rapidjson::SizeType idx = 0; idx < domains.Size(); ++idx) {
            const auto& pd_obj = domains[idx];
            std::string ctx = "power_domains[" + std::to_string(idx) + "]";

            auto pd_id = get_uint64(pd_obj, "id", ctx.c_str());

            std::vector<CStateLevel> c_states;
            if (pd_obj.HasMember("c_states")) {
                const auto& states = pd_obj["c_states"];
                if (states.IsArray()) {
                    for (rapidjson::SizeType sidx = 0; sidx < states.Size(); ++sidx) {
                        const auto& cs = states[sidx];
                        std::string cs_ctx = ctx + ".c_states[" + std::to_string(sidx) + "]";

                        int level = get_int(cs, "level", cs_ctx.c_str());
                        double power = get_double(cs, "power_mw", cs_ctx.c_str());
                        double latency = get_double_or(cs, "latency_us", 0.0);

                        CStateScope scope = CStateScope::PerProcessor;
                        if (cs.HasMember("scope")) {
                            std::string scope_str = cs["scope"].GetString();
                            if (scope_str == "domain_wide") {
                                scope = CStateScope::DomainWide;
                            }
                        }

                        c_states.push_back(CStateLevel{
                            level,
                            scope,
                            duration_from_seconds(latency / 1e6),  // Convert us to seconds
                            Power{power}
                        });
                    }
                }
            }

            // Default C0 state if none specified
            if (c_states.empty()) {
                c_states.push_back(CStateLevel{
                    0,
                    CStateScope::PerProcessor,
                    Duration::zero(),
                    Power{0.0}
                });
            }

            auto& pd = platform.add_power_domain(std::move(c_states));
            power_domains[pd_id] = &pd;
        }
    }

    // Load processors
    if (doc.HasMember("processors")) {
        const auto& procs = get_array(doc, "processors", "platform");
        for (rapidjson::SizeType idx = 0; idx < procs.Size(); ++idx) {
            const auto& proc_obj = procs[idx];
            std::string ctx = "processors[" + std::to_string(idx) + "]";

            std::string type_name = get_string(proc_obj, "type", ctx.c_str());
            auto cd_id = get_uint64(proc_obj, "clock_domain", ctx.c_str());
            auto pd_id = get_uint64(proc_obj, "power_domain", ctx.c_str());

            // Validate references
            auto type_it = processor_types.find(type_name);
            if (type_it == processor_types.end()) {
                throw LoaderError("unknown processor type '" + type_name + "'", ctx.c_str());
            }

            auto cd_it = clock_domains.find(cd_id);
            if (cd_it == clock_domains.end()) {
                throw LoaderError("unknown clock_domain " + std::to_string(cd_id), ctx.c_str());
            }

            auto pd_it = power_domains.find(pd_id);
            if (pd_it == power_domains.end()) {
                throw LoaderError("unknown power_domain " + std::to_string(pd_id), ctx.c_str());
            }

            platform.add_processor(*type_it->second, *cd_it->second, *pd_it->second);
        }
    }
}

// Load legacy cluster format
void load_legacy_format(Engine& engine, const rapidjson::Document& doc) {
    auto& platform = engine.platform();

    const auto& clusters = get_array(doc, "clusters", "platform");

    for (rapidjson::SizeType cidx = 0; cidx < clusters.Size(); ++cidx) {
        const auto& cluster = clusters[cidx];
        std::string ctx = "clusters[" + std::to_string(cidx) + "]";

        int num_procs = get_int(cluster, "procs", ctx.c_str());
        double perf_score = get_double_or(cluster, "perf_score", 1.0);
        double effective_freq = get_double_or(cluster, "effective_freq", 1000.0);

        // Get frequencies array
        double freq_min = effective_freq;
        double freq_max = effective_freq;

        if (cluster.HasMember("frequencies")) {
            const auto& freqs = cluster["frequencies"];
            if (freqs.IsArray() && !freqs.Empty()) {
                freq_min = freqs[0].GetDouble();
                freq_max = freqs[freqs.Size() - 1].GetDouble();
            }
        }

        // Create processor type for this cluster
        std::string type_name = "cluster" + std::to_string(cidx);
        auto& proc_type = platform.add_processor_type(type_name, perf_score);

        // Create clock domain for this cluster
        auto& cd = platform.add_clock_domain(Frequency{freq_min}, Frequency{freq_max});
        cd.set_frequency(Frequency{effective_freq});

        // Set discrete frequency modes from frequencies array
        if (cluster.HasMember("frequencies")) {
            const auto& freqs = cluster["frequencies"];
            if (freqs.IsArray() && freqs.Size() > 1) {
                std::vector<Frequency> modes;
                modes.reserve(freqs.Size());
                for (rapidjson::SizeType fi = 0; fi < freqs.Size(); ++fi) {
                    modes.push_back(Frequency{freqs[fi].GetDouble()});
                }
                cd.set_frequency_modes(std::move(modes));
            }
        }

        // Set effective frequency
        cd.set_freq_eff(Frequency{effective_freq});

        // Set power model if specified
        if (cluster.HasMember("power_model")) {
            const auto& pm = cluster["power_model"];
            if (pm.IsArray() && pm.Size() == 4) {
                std::array<double, 4> coeffs = {
                    pm[0].GetDouble(),
                    pm[1].GetDouble(),
                    pm[2].GetDouble(),
                    pm[3].GetDouble()
                };
                cd.set_power_coefficients(coeffs);
            }
        }

        // Create default power domain with C0 state
        std::vector<CStateLevel> c_states;
        c_states.push_back(CStateLevel{
            0,
            CStateScope::PerProcessor,
            Duration::zero(),
            Power{0.0}
        });
        auto& pd = platform.add_power_domain(std::move(c_states));

        // Create processors for this cluster
        for (int pidx = 0; pidx < num_procs; ++pidx) {
            platform.add_processor(proc_type, cd, pd);
        }
    }
}

} // anonymous namespace

void load_platform(Engine& engine, const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw LoaderError("cannot open file", path.string());
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    load_platform_from_string(engine, oss.str());
}

void load_platform_from_string(Engine& engine, std::string_view json) {
    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());

    if (doc.HasParseError()) {
        throw LoaderError(
            std::string("JSON parse error: ") + rapidjson::GetParseError_En(doc.GetParseError()),
            "at offset " + std::to_string(doc.GetErrorOffset()));
    }

    if (!doc.IsObject()) {
        throw LoaderError("root must be an object", "platform");
    }

    // Detect format: legacy has "clusters", new format has "processor_types" or "processors"
    if (doc.HasMember("clusters")) {
        load_legacy_format(engine, doc);
    } else {
        load_new_format(engine, doc);
    }
}

} // namespace schedsim::io
