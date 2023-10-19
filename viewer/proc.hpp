#ifndef PROC_HPP
#define PROC_HPP

#include "path.hpp"
#include <vector>

class proc {
      private:
        double pos_y;
        std::vector<path*> paths;

      public:
        void add(path* pth) { paths.push_back(pth); };
        void set_y(const double& m_pos_y) { pos_y = m_pos_y; };
        auto print() -> std::string;
};

#endif
