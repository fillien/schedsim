#include "rtsched.hpp"
#include "gantt.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {
auto operator<<(std::ostream& out, const outputs::gantt::arrival& evt) -> std::ostream&
{
        return out << "\\TaskArrival{" << evt.index << "}{" << evt.timestamp << "}";
}

auto operator<<(std::ostream& out, const outputs::gantt::deadline& evt) -> std::ostream&
{
        return out << "\\TaskDeadline{" << evt.index << "}{" << evt.timestamp << "}";
}

auto operator<<(std::ostream& out, const outputs::gantt::execution& evt) -> std::ostream&
{
        using namespace outputs::gantt;
        return out << "\\TaskExecution[color=" << get_color_name(evt.cpu) << "]{" << evt.index
                   << "}{" << evt.start << "}{" << evt.stop << "}";
}

auto operator<<(std::ostream& out, const outputs::gantt::active_non_cont& evt) -> std::ostream&
{
        return out << "\\TaskRespTime{" << evt.index << "}{" << evt.start << "}{" << evt.stop
                   << "}";
}

auto operator<<(std::ostream& out, const outputs::gantt::command& cmd) -> std::ostream&
{
        std::visit([&](auto&& cmd) { out << cmd; }, cmd);
        return out;
}
}; // namespace

namespace outputs::gantt::rtsched {
auto draw(const outputs::gantt::gantt& chart) -> std::string
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
