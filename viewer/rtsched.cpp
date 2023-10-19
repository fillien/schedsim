#include "rtsched.hpp"
#include <sstream>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace rtsched {

auto serialize(const command& com) -> std::string {
        return std::visit(
            overloaded{[](const TaskArrival& com) {
                               std::ostringstream out;
                               out << "\\TaskArrival{" << com.index << "}{" << com.arrival << "}";
                               return out.str();
                       },
                       [](const TaskDeadline& com) {
                               std::ostringstream out;
                               out << "\\TaskDeadline{" << com.index << "}{" << com.deadline << "}";
                               return out.str();
                       },
                       [](const TaskExecution& com) {
                               std::ostringstream out;
                               out << "\\TaskExecution{" << com.index << "}{" << com.start << "}{"
                                   << com.stop << "}";
                               return out.str();
                       },
                       [](const TaskEnd& com) {
                               std::ostringstream out;
                               out << "\\TaskEnd{" << com.index << "}{" << com.stop << "}";
                               return out.str();
                       }},
            com);
}

	auto grid_print(const struct grid& grid) -> std::string {
        std::ostringstream out;
        out << "\\begin{RTGrid}{" << grid.nb_axis << "}{" << grid.duration << "}\n";
        for (const auto& com : grid.commands) {
                out << serialize(com) << '\n';
        }
        out << "\\end{RTGrid}\n";
        return out.str();
}

} // namespace rtsched
