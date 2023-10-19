#ifndef DEADLINE_HPP
#define DEADLINE_HPP

#include "path.hpp"

class deadline : public path {
      public:
        double time;

        explicit deadline(const double& pos_x, const double& pos_y, const double& time)
            : path(pos_x, pos_y), time(time){};

        auto print() const -> std::string override;
};

#endif
