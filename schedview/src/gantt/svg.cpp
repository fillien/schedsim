#include "svg.hpp"
#include "gantt.hpp"

#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>

namespace {
constexpr double AXIS_HEIGHT{64};
constexpr double TIME_UNIT{100};
constexpr double OFFSET_X{30};
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

auto operator<<(std::ostream& out, const outputs::gantt::arrival& evt) -> std::ostream&
{
        const double OFFSET_Y{AXIS_HEIGHT * static_cast<double>(evt.index)};
        out << "<line class='event' x1='" << OFFSET_X + TIME_UNIT * evt.timestamp << "' x2='"
            << OFFSET_X + TIME_UNIT * evt.timestamp;
        out << "' y1='" << OFFSET_Y << "' y2='" << OFFSET_Y - EVENT_HEIGHT;
        out << "'><title>arrival: " << evt.timestamp << "</title>";
        out << "</line>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::deadline& evt) -> std::ostream&
{
        const double OFFSET_Y{AXIS_HEIGHT * static_cast<double>(evt.index)};
        out << "<line class='event' x1='" << OFFSET_X + TIME_UNIT * evt.timestamp << "' x2='"
            << OFFSET_X + TIME_UNIT * evt.timestamp;
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
        out << "<rect class='task' x='" << OFFSET_X + TIME_UNIT * evt.start << "' y='" << TASK_OFFSET_Y;
        out << "' width='" << TIME_UNIT * DURATION << "' height='" << TASK_HEIGHT_MAX;
        out << "' fill='" << outputs::gantt::get_color_hex(evt.cpu) << "'>";
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
        out << "<rect class='anc' x='" << OFFSET_X + TIME_UNIT * evt.start << "' y='" << TASK_OFFSET_Y;
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
}; // namespace

auto outputs::gantt::svg::draw(const outputs::gantt::gantt& chart) -> std::string
{
        std::stringstream out;
        out << "<svg viewBox='0 0 " << OFFSET_X + chart.duration * TIME_UNIT;
        out << " " << static_cast<double>(chart.nb_axis) * AXIS_HEIGHT
            << "' xmlns='http://www.w3.org/2000/svg'>\n";
        out << defs << style << '\n';
        for (std::size_t i = 1; i <= chart.nb_axis; ++i) {
                auto axis = static_cast<double>(i);
		auto BASELINE {AXIS_HEIGHT * axis};
		out << "<text x='0' y='" << BASELINE - (AXIS_HEIGHT/4) << "'>" << TAU_SYM << i << "</text>";
                out << "<line x1='" << OFFSET_X << "' y1='" << BASELINE;
                out << "' x2='" << OFFSET_X + chart.duration * TIME_UNIT;
                out << "' y2='" << BASELINE << "' stroke='black' stroke-width='1'/>\n";
        }
        out << chart.commands;
        out << "</svg>\n";
        return out.str();
}
