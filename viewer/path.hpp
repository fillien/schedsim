#ifndef PATH_HPP
#define PATH_HPP

#include <string>

class path {
      protected:
        double pos_x;
        double pos_y;

      public:
        path(const double& pos_x, const double& pos_y) : pos_x(pos_x), pos_y(pos_y){};
        virtual ~path() = default;
        auto set_y(const double& m_pos_y) { pos_y = m_pos_y; };
        virtual auto print() const -> std::string = 0;
};

#endif
