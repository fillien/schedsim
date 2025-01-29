#include <gantt/gantt.hpp>
#include <gantt/svg.hpp>

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <variant>

#include <iostream>

namespace {
template <class... Ts> struct overload : Ts... {
        using Ts::operator()...;
};
template <class... Ts> overload(Ts...) -> overload<Ts...>;

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
                  ".task { stroke: black; stroke-width: 0.5px; }"
                  ".anc { stroke: black; stroke-width: 1px; fill: url(#bars); }"
                  ".timestamp { font-size: 10px; text-anchor: middle; }"
                  "</style>"};

auto print_order(const outputs::gantt::command& first, const outputs::gantt::command& second)
    -> bool
{
        using namespace outputs::gantt;
        constexpr auto get_z_index = overload{
            [](const finished&) { return 2; },
            [](const arrival&) { return 1; },
            [](const deadline&) { return 1; },
            [](const auto&) { return 0; }};

        return std::visit(get_z_index, first) < std::visit(get_z_index, second);
}

auto operator<<(std::ostream& out, const outputs::gantt::arrival& evt) -> std::ostream&
{
        const double OFFSET_Y{AXIS_HEIGHT * static_cast<double>(evt.index)};
        out << "<line class='event' x1='" << OFFSET_X + TIME_UNIT * evt.timestamp << "' x2='"
            << OFFSET_X + TIME_UNIT * evt.timestamp << "' y1='" << OFFSET_Y << "' y2='"
            << OFFSET_Y - EVENT_HEIGHT << "'><title>arrival: " << evt.timestamp << "</title>"
            << "</line>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::deadline& evt) -> std::ostream&
{
        const double OFFSET_Y{AXIS_HEIGHT * static_cast<double>(evt.index)};
        out << "<line class='event' x1='" << OFFSET_X + TIME_UNIT * evt.timestamp << "' x2='"
            << OFFSET_X + TIME_UNIT * evt.timestamp << "' y1='" << OFFSET_Y - EVENT_HEIGHT
            << "' y2='" << OFFSET_Y << "'><title>deadline: " << evt.timestamp << "</title>"
            << "</line>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::finished& evt) -> std::ostream&
{
        const double TIMESTAMP{OFFSET_X + TIME_UNIT * evt.timestamp};
        const double INDEX{AXIS_HEIGHT * static_cast<double>(evt.index)};
        out << "<circle cx='" << TIMESTAMP << "' cy='" << INDEX
            << "' r='5' fill='white' stroke='black' stroke-width='2'/>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::execution& evt) -> std::ostream&
{
        auto normalize = [&evt](double freq) -> double {
                return (freq - evt.frequency_min) / (evt.frequency_max - evt.frequency_min);
        };
        constexpr double TASK_HEIGHT_MAX{30};
        const double TASK_HEIGHT{TASK_HEIGHT_MAX * (normalize(evt.frequency) / 1)};
        const double TASK_OFFSET_Y{static_cast<double>(evt.index - 1) * AXIS_HEIGHT + 33};
        const double DURATION{evt.stop - evt.start};

        out << "<rect class='task' x='" << OFFSET_X + TIME_UNIT * evt.start << "' y='"
            << TASK_OFFSET_Y + TASK_HEIGHT_MAX - TASK_HEIGHT << "' width='" << TIME_UNIT * DURATION
            << "' height='" << TASK_HEIGHT << "' fill='" << outputs::gantt::get_color_hex(evt.cpu)
            << "'>"
            << "<title>"
            << "start: " << evt.start << NEWLINE << "stop: " << evt.stop << NEWLINE
            << "duration: " << DURATION << NEWLINE << "freq: " << evt.frequency << "</title>"
            << "</rect>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::active_non_cont& evt) -> std::ostream&
{
        constexpr double TASK_HEIGHT_MAX{30};
        const double TASK_OFFSET_Y{static_cast<double>(evt.index - 1) * AXIS_HEIGHT + 33};
        const double DURATION{evt.stop - evt.start};
        out << "<rect class='anc' x='" << OFFSET_X + TIME_UNIT * evt.start << "' y='"
            << TASK_OFFSET_Y << "' width='" << TIME_UNIT * DURATION << "' height='"
            << TASK_HEIGHT_MAX << "'>"
            << "<title>"
            << "start: " << evt.start << NEWLINE << "stop: " << evt.start + DURATION << NEWLINE
            << "duration: " << DURATION << "</title>"
            << "</rect>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::proc_mode_idle& evt) -> std::ostream&
{
        constexpr double TASK_HEIGHT_MAX{30};
        const double TASK_OFFSET_Y{static_cast<double>(evt.index - 1) * AXIS_HEIGHT + 33};
        const double DURATION{evt.stop - evt.start};
        out << "<rect class='task' x='" << OFFSET_X + TIME_UNIT * evt.start << "' y='"
            << TASK_OFFSET_Y << "' width='" << TIME_UNIT * DURATION << "' height='"
            << TASK_HEIGHT_MAX << "' fill='" << outputs::gantt::get_color_hex(5) << "'>"
            << "<title>"
            << "start: " << evt.start << NEWLINE << "stop: " << evt.start + DURATION << NEWLINE
            << "duration: " << DURATION << "</title>"
            << "</rect>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::proc_mode_running& evt) -> std::ostream&
{
        constexpr double TASK_HEIGHT_MAX{30};
        const double TASK_OFFSET_Y{static_cast<double>(evt.index - 1) * AXIS_HEIGHT + 33};
        const double DURATION{evt.stop - evt.start};
        out << "<rect class='task' x='" << OFFSET_X + TIME_UNIT * evt.start << "' y='"
            << TASK_OFFSET_Y << "' width='" << TIME_UNIT * DURATION << "' height='"
            << TASK_HEIGHT_MAX << "' fill='" << outputs::gantt::get_color_hex(1) << "'>"
            << "<title>"
            << "start: " << evt.start << NEWLINE << "stop: " << evt.start + DURATION << NEWLINE
            << "duration: " << DURATION << "</title>"
            << "</rect>";
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::proc_mode_sleep& evt) -> std::ostream&
{
        constexpr double TASK_HEIGHT_MAX{30};
        const double TASK_OFFSET_Y{static_cast<double>(evt.index - 1) * AXIS_HEIGHT + 33};
        const double DURATION{evt.stop - evt.start};
        out << "<rect class='task' x='" << OFFSET_X + TIME_UNIT * evt.start << "' y='"
            << TASK_OFFSET_Y << "' width='" << TIME_UNIT * DURATION << "' height='"
            << TASK_HEIGHT_MAX << "' fill='" << outputs::gantt::get_color_hex(2) << "'>"
            << "<title>"
            << "start: " << evt.start << NEWLINE << "stop: " << evt.start + DURATION << NEWLINE
            << "duration: " << DURATION << "</title>"
            << "</rect>";
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

auto outputs::gantt::svg::draw(const outputs::gantt::gantt& input) -> std::string
{
        std::stringstream out;

        outputs::gantt::gantt chart = input;

        std::sort(chart.commands.begin(), chart.commands.end(), print_order);

        const auto NB_AXIS{static_cast<double>(chart.nb_axis)};
        const auto CHART_WIDTH{OFFSET_X + chart.duration * TIME_UNIT};

        out << "<svg width='" << CHART_WIDTH << "' viewBox='0 0 " << CHART_WIDTH << " "
            << 35 + NB_AXIS * AXIS_HEIGHT << "' xmlns='http://www.w3.org/2000/svg'>\n"
            << defs << style << '\n';

        const double GANTT_HEIGHT{10 + NB_AXIS * AXIS_HEIGHT};

        for (auto i = 0; i <= chart.duration; ++i) {
                out << "<line x1='" << OFFSET_X + TIME_UNIT * i << "' y1='0' x2='"
                    << OFFSET_X + TIME_UNIT * i << "' y2='" << GANTT_HEIGHT
                    << "' stroke='grey'/>\n";
                out << "<text text-anchor='middle' x='" << OFFSET_X + TIME_UNIT * i << "' y='"
                    << GANTT_HEIGHT + 15 << "'>" << i << "</text>\n";
        }

        for (std::size_t i = 1; i <= chart.nb_axis; ++i) {
                auto axis = static_cast<double>(i);
                auto BASELINE{AXIS_HEIGHT * axis};
                out << "<text x='0' y='" << BASELINE - (AXIS_HEIGHT / 4) << "'>" << TAU_SYM << i
                    << "</text>"
                    << "<line x1='" << OFFSET_X << "' y1='" << BASELINE << "' x2='"
                    << OFFSET_X + chart.duration * TIME_UNIT << "' y2='" << BASELINE
                    << "' stroke='black' stroke-width='1'/>\n";
        }
        out << chart.commands << "</svg>\n";
        return out.str();
}

auto outputs::gantt::html::draw(const outputs::gantt::gantt& chart) -> std::string
{
        constexpr auto HTML_HEADER{"<!DOCTYPE html><html><head></head><body>"};
        constexpr auto HTML_FOOTER{"</body></html>"};
        std::stringstream out;
        out << HTML_HEADER << outputs::gantt::svg::draw(chart) << HTML_FOOTER;
        return out.str();
}
