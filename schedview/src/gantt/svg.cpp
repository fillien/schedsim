#include "svg.hpp"
#include "gantt.hpp"
#include <array>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>

constexpr double AXIS_HEIGHT{64};
constexpr double TIME_UNIT{100};
const double EVENT_HEIGHT{40};

constexpr auto TAU_SYM{"&#120591;"};
constexpr auto NEWLINE{"&#xA;"};

std::string defs{"<defs>"
                 "<marker id='arrow' viewBox='0 0 10 10' refX='5' refY='5' "
                 "markerWidth='6' markerHeight='6' orient='auto-start-reverse'>"
                 "<path d='M 0 0 L 10 5 L 0 10 z'/>"
                 "</marker>"
                 "<pattern id='bars' width='10' height='10' patternUnits='userSpaceOnUse'>"
                 "<line x1='-5' y1='5' x2='5' y2='15' stroke='black' stroke-width='2'/>"
                 "<line x1='0' y1='0' x2='10' y2='10' stroke='black' stroke-width='2'/>"
                 "<line x1='5' y1='-5' x2='15' y2='5' stroke='black' stroke-width='2'/>"
                 "</pattern>"
                 "</defs>"};
std::string style{"<style>"
                  ".event { stroke: black; stroke-width: 1.5px; marker-end: url(#arrow); }"
                  ".task { stroke: black; stroke-width: 1px; }"
                  ".anc { stroke: black; stroke-width: 1px; fill: url(#bars); }"
                  ".timestamp { font-size: 10px; text-anchor: middle; }"
                  "</style>"};

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

auto get_color_name(std::size_t index) -> std::string { return colors.at(index).first; }

auto get_color_hex(std::size_t index) -> std::string { return colors.at(index).second; }

auto operator<<(std::ostream& out, const outputs::gantt::arrival& evt) -> std::ostream&
{
        const double OFFSET_Y{AXIS_HEIGHT * static_cast<double>(evt.index)};
        out << "<line class='event' x1='" << TIME_UNIT * evt.timestamp << "' x2='"
            << TIME_UNIT * evt.timestamp;
        out << "' y1='" << OFFSET_Y << "' y2='" << OFFSET_Y - EVENT_HEIGHT;
        out << "'><title>arrival: " << evt.timestamp << "</title>";
        out << "</line>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::deadline& evt) -> std::ostream&
{
        const double OFFSET_Y{AXIS_HEIGHT * static_cast<double>(evt.index)};
        out << "<line class='event' x1='" << TIME_UNIT * evt.timestamp << "' x2='"
            << TIME_UNIT * evt.timestamp;
        out << "' y1='" << OFFSET_Y - EVENT_HEIGHT << "' y2='" << OFFSET_Y;
        out << "'><title>deadline: " << evt.timestamp << "</title>";
        out << "</line>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::execution& evt) -> std::ostream&
{
        constexpr double TASK_HEIGHT_MAX{30};
        const double TASK_OFFSET_Y{static_cast<double>(evt.index - 1) * AXIS_HEIGHT + 33};
        const double DURATION{evt.stop - evt.start};
        out << "<rect class='task' x='" << TIME_UNIT * evt.start << "' y='" << TASK_OFFSET_Y;
        out << "' width='" << TIME_UNIT * DURATION << "' height='" << TASK_HEIGHT_MAX;
        out << "' fill='" << get_color_hex(evt.cpu) << "'>";
        out << "<title>";
        out << "start: " << evt.start << NEWLINE;
        out << "stop: " << evt.stop << NEWLINE;
        out << "duration: " << DURATION;
        out << "</title>";
        out << "</rect>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::active_non_cont& evt) -> std::ostream&
{
        constexpr double TASK_HEIGHT_MAX{30};
        const double TASK_OFFSET_Y{static_cast<double>(evt.index - 1) * AXIS_HEIGHT + 33};
        const double DURATION{evt.stop - evt.start};
        out << "<rect class='anc' x='" << TIME_UNIT * evt.start << "' y='" << TASK_OFFSET_Y;
        out << "' width='" << TIME_UNIT * DURATION << "' height='" << TASK_HEIGHT_MAX;
        out << "'>";
        out << "<title>";
        out << "start: " << evt.start << NEWLINE;
        out << "stop: " << evt.start + DURATION << NEWLINE;
        out << "duration: " << DURATION;
        out << "</title>";
        out << "</rect>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::command& cmd) -> std::ostream&
{
        std::visit([&](auto&& arg) { out << arg << '\n'; }, cmd);
        return out;
}

template <typename T> auto operator<<(std::ostream& out, const std::vector<T>& vec) -> std::ostream&
{
        for (size_t i = 0; i < vec.size(); ++i) {
                out << vec.at(i);
        }
        return out;
}

auto outputs::gantt::svg::draw(const outputs::gantt::gantt& chart) -> std::string
{
        std::stringstream out;
        out << "<svg viewBox='0 0 " << chart.duration * TIME_UNIT;
        out << " " << static_cast<double>(chart.nb_axis) * AXIS_HEIGHT
            << "' xmlns='http://www.w3.org/2000/svg'>\n";
        out << defs << style << '\n';
        for (std::size_t i = 1; i <= chart.nb_axis; ++i) {
                auto axis = static_cast<double>(i);
                out << "<line x1='0' y1='" << AXIS_HEIGHT * axis;
                out << "' x2='" << chart.duration * TIME_UNIT;
                out << "' y2='" << AXIS_HEIGHT * axis << "' stroke='black' stroke-width='1'/>\n";
        }
        out << chart.commands;
        out << "</svg>\n";
        return out.str();
}
