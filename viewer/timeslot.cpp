#include "timeslot.hpp"

#include <sstream>

auto timeslot::print() const -> std::string {
        std::ostringstream out{};

        out << "<rect "
            << "style=\"fill:" << color
            << ";fill-opacity:1;stroke:none;stroke-width:0.38869;stroke-dasharray:none;paint-order:"
               "fill markers stroke\" "
            << "width=\"" << duration << "\" "
            << "height=\"9.1653318\" "
            << "x=\"" << pos_x << "\" "
            << "y=\"" << pos_y << "\" />";

        return out.str();
}
