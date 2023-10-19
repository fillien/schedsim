#include "deadline.hpp"

#include <sstream>

auto deadline::print() const -> std::string {
        std::ostringstream out{};

        out << "<g transform=\"translate(" << pos_x << "," << pos_y
            << ")\"><text xml:space=\"preserve\" "
            << "style=\"font-weight:bold;font-size:3.88056px;font-family:'FantasqueSansM Nerd Font "
               "Mono';-inkscape-font-specification:'FantasqueSansM Nerd Font Mono "
               "Bold';fill:#000000;fill-opacity:1;stroke:none;stroke-width:0.264999;stroke-"
               "dasharray:none;paint-order:fill markers stroke\" "
            << "x=\"0\" y=\"18.163998\">" << time << "</text><path "
            << "style=\"fill:#000000;fill-opacity:1;stroke:none;stroke-width:0.264999;stroke-"
               "dasharray:none;paint-order:fill markers stroke\" "
            << "d=\"m 19.914273,40.782903 v 2.770701 h -0.304858 l 0.457287,0.527699 "
               "0.457287,-0.527699 h -0.304858 v -2.770701 z\" "
            << "transform=\"matrix(2.7046312,0,0,4.2877153,-53.3152043,-174.86548)\" /></g>";

        return out.str();
}
