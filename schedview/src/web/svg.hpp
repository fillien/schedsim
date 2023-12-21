#include <sstream>

#include "arrival.hpp"
#include "deadline.hpp"
#include "path.hpp"
#include "timeslot.hpp"

#include "proc.hpp"

class svg_document {
      public:
        std::vector<proc*> procs;

        friend auto operator<<(std::ostream& out, const svg_document& svg) -> std::ostream&;
};

auto operator<<(std::ostream& out, const svg_document& svg) -> std::ostream& {
        out << "<svg width=\"297mm\" height=\"210mm\" viewBox=\"0 0 297 210\" "
               "version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" "
               "xmlns:svg=\"http://www.w3.org/2000/svg\">\n";

        for (auto const& pth : svg.procs) {
                out << pth->print();
        }

        out << "</svg>\n";

        return out;
};
