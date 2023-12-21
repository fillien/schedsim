#include "arrival.hpp"

#include <sstream>

auto arrival::print() const -> std::string {
        std::ostringstream out{};
        out << "<g transform=\"translate(" << pos_x << "," << pos_y
            << ")\">\n"
               "<path "
               "style=\"fill:#000000;fill-opacity:1;stroke:none;stroke-width:0.902425;"
               "stroke-dasharray:none;paint-order:fill markers stroke\" "
               "d=\"m 0.824528,14.142601 c 0,-3.959993 0,-7.919985 0,-11.879977 -0.274842,0 "
               "-0.549686,0 -0.824528,0 0.412264,-0.75421 0.824528,-1.508414 1.236793,-2.262624 "
               "0.412264,0.75421 0.824528,1.508414 1.236792,2.262624 -0.274842,0 -0.549686,0 "
               "-0.824528,0 0,3.959992 0,7.919984 0,11.879977 -0.274842,0 -0.549687,0 -0.824529,0 "
               "z\"/>\n"
               "<text xml:space=\"preserve\" "
               "style=\"font-weight:bold;font-size:3.88056px;font-family:'FantasqueSansM Nerd Font "
               "Mono';-inkscape-font-specification:'FantasqueSansM Nerd Font Mono "
               "Bold';fill:#000000;fill-opacity:1;stroke:none;stroke-width:0.264999;stroke-"
               "dasharray:none;paint-order:fill markers stroke\" "
               "x=\"0.27897\" "
               "y=\"18.163997\">"
            << time
            << "</text>\n"
               "</g>";
        return out.str();
};
