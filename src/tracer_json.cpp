#include "tracer_json.hpp"
#include "nlohmann/json.hpp"

#include <sstream>
#include <string>

auto print_json(const std::pair<double, events::event>& evt) -> std::string {
        std::ostringstream out;
        auto logs = nlohmann::json(std::visit(log_json{}, evt.second));
        logs.push_back({"time", evt.first});
        out << logs.dump() << std::endl;
        return out.str();
}
