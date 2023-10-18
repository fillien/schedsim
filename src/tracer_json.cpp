#include "tracer_json.hpp"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

#include <sstream>
#include <string>

// auto print_json(const std::pair<double, events::event>& evt) -> std::string {
auto print_json(const std::multimap<double, events::event>& log) -> std::string {
        using json = nlohmann::json;

        std::ostringstream out;
        auto serialize_json = json::array({});

        for (const auto& trace : log) {
                auto trace_json = json(std::visit(log_json{}, trace.second));
                trace_json.push_back({"time", trace.first});
                serialize_json.push_back(trace_json);
        }
        out << serialize_json.dump() << std::endl;
        return out.str();
}
