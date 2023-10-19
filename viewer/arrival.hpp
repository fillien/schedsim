#ifndef ARRIVAL_HPP
#define ARRIVAL_HPP

#include "path.hpp"

class arrival : public path {
      private:
        double time;

      public:
        explicit arrival(const double& pos_x, const double& pos_y, const double& time)
            : path(pos_x, pos_y), time(time){};
        auto print() const -> std::string override;
};

#endif
