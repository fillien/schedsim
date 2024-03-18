#ifndef GANTT_HPP
#define GANTT_HPP

#include <array>
#include <map>
#include <protocols/hardware.hpp>
#include <protocols/traces.hpp>
#include <variant>
#include <vector>

namespace outputs::gantt {

constexpr std::array<std::pair<const char*, const char*>, 19> colors = {
    {{"red", "#FF0000"},
     {"green", "#008000"},
     {"blue", "#0000FF"},
     {"cyan", "#00FFFF"},
     {"magenta", "#FF00FF"},
     {"yellow", "#FFFF00"},
     {"black", "#000000"},
     {"gray", "#808080"},
     {"white", "#FFFFFF"},
     {"darkgray", "#A9A9A9"},
     {"lightgray", "#D3D3D3"},
     {"brown", "#A52A2A"},
     {"lime", "#00FF00"},
     {"olive", "#808000"},
     {"orange", "#FFA500"},
     {"pink", "#FFC0CB"},
     {"purple", "#800080"},
     {"teal", "#008080"},
     {"violet", "#EE82EE"}}};

inline auto get_color_name(std::size_t index) -> std::string { return colors.at(index).first; }

inline auto get_color_hex(std::size_t index) -> std::string { return colors.at(index).second; }

struct arrival {
        std::size_t index;
        double timestamp;
};

struct deadline {
        std::size_t index;
        double timestamp;
};

struct finished {
        std::size_t index;
        double timestamp;
};

struct execution {
        std::size_t index;
        std::size_t cpu;
        double start;
        double stop;
        double frequency;
};

struct active_non_cont {
        std::size_t index;
        double start;
        double stop;
};

using command = std::variant<arrival, deadline, finished, execution, active_non_cont>;

struct gantt {
        std::size_t nb_axis;
        double duration;
        std::vector<command> commands;
};

auto generate_gantt(
    const std::multimap<double, protocols::traces::trace>& logs,
    const protocols::hardware::hardware& platform) -> gantt;
}; // namespace outputs::gantt

#endif
