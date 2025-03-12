#include <analyzers/gantt/gantt.hpp>
#include <analyzers/gantt/rtsched.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>

namespace {
auto operator<<(std::ostream& out, const outputs::gantt::Arrival& evt) -> std::ostream&
{
        return out << "\\TaskArrival{" << evt.index << "}{" << evt.timestamp << "}";
}

auto operator<<(std::ostream& out, const outputs::gantt::Deadline& evt) -> std::ostream&
{
        return out << "\\TaskDeadline{" << evt.index << "}{" << evt.timestamp << "}";
}

auto operator<<(std::ostream& out, const outputs::gantt::Execution& evt) -> std::ostream&
{
        using namespace outputs::gantt;
        return out << "\\TaskExecution[color=" << get_color_name(evt.cpu) << "]{" << evt.index
                   << "}{" << evt.start << "}{" << evt.stop << "}";
}

auto operator<<(std::ostream& out, const outputs::gantt::ActiveNonCont& evt) -> std::ostream&
{
        return out << "\\TaskRespTime{" << evt.index << "}{" << evt.start << "}{" << evt.stop
                   << "}";
}

auto operator<<(std::ostream& out, [[maybe_unused]] const outputs::gantt::Finished& evt)
    -> std::ostream&
{
        return out;
}

auto operator<<(std::ostream& out, const outputs::gantt::command& cmd) -> std::ostream&
{
        std::visit([&](auto&& cmd) { out << cmd; }, cmd);
        return out;
}
}; // namespace

namespace outputs::gantt::rtsched {
auto draw(const outputs::gantt::Gantt& chart) -> std::string
{
        std::stringstream out;
        out << "\\begin{RTGrid}{" << chart.nb_axis << "}{" << chart.duration << "}\n";
        for (const auto& cmd : chart.commands) {
                out << cmd << '\n';
        }
        out << "\\end{RTGrid}\n";
        return out.str();
}

}; // namespace outputs::gantt::rtsched
