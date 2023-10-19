#include "proc.hpp"

#include <sstream>
#include <string>

auto proc::print() -> std::string {
        std::ostringstream out{};

        for (auto const& pth : paths) {
                pth->set_y(pos_y);
                out << pth->print() << '\n';
        }

        return out.str();
}
