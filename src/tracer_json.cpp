#include "tracer_json.hpp"
#include "event.hpp"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

#include <ios>
#include <sstream>
#include <string>
#include <fstream>

void prepare_log_file()
{
        std::ofstream out;
        out.open("out.json");
	while(!out.is_open()){}
        out << "[";
	out.close();
}

void write_log_file(std::multimap<double, events::event>& logs)
{
	using json = nlohmann::json;

        std::ofstream out;
        out.open("out.json", std::ios_base::app);
        while (!out.is_open()) {}

        for (auto itr = std::begin(logs); itr != std::end(logs); ++itr) {
		auto trace_json = json(std::visit(log_json{}, (*itr).second));
                trace_json.push_back({"time", (*itr).first});
                out << trace_json.dump();
        }

	logs.clear();

	out.close();
}

void finish_log_file()
{
        std::ofstream out;
        out.open("out.json", std::ios_base::app);
        while (!out.is_open()) {}
        out << "]";
	out.close();
}
