#include "analyzers/system_utilization.hpp"
#include "protocols/traces.hpp"
#include <iostream>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
        using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace outputs::sys_util {
void print_active_utilization(const std::vector<std::pair<double, protocols::traces::trace>>& input)
{
        double current_active_utilization{0};

        std::cout << "timestamp active_utilization\n0 0\n";
        for (const auto& tra : input) {
                const auto& [timestamp, event] = tra;

                std::visit(
                    overloaded{
                        [&](protocols::traces::ServReady evt) {
                                current_active_utilization += evt.utilization;
                                std::cout << timestamp << ' ' << current_active_utilization << '\n';
                        },
                        [&](protocols::traces::ServInactive evt) {
                                current_active_utilization -= evt.utilization;
                                std::cout << timestamp << ' ' << current_active_utilization << '\n';
                        },
                        [](auto) {}},
                    event);
        }
}
} // namespace outputs::sys_util
