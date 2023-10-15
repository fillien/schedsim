#include "tracer_json.hpp"
#include "nlohmann/json.hpp"

#include <string>

void print_json(const std::pair<double, events::event>& evt) {
        auto out = nlohmann::json(std::visit(log_json{}, evt.second));
        out.push_back({"time", evt.first});
        std::cout << out.dump() << std::endl;
}
