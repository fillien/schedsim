#ifndef TIMESLOT_HPP
#define TIMESLOT_HPP

#include "path.hpp"

#include <string>

class timeslot : public path {
      private:
        double duration;
        std::string color;

      public:
        explicit timeslot(const double& pos_x, const double& pos_y, const double& duration,
                          const std::string& color)
            : path(pos_x, pos_y), duration(duration), color(std::move(color)){};
        auto print() const -> std::string override;
};

class timeslot_active_ready : public timeslot {
      public:
        explicit timeslot_active_ready(const double& pos_x, const double& pos_y,
                                       const double& duration)
            : timeslot(pos_x, pos_y, duration, "#d35f5fff"){};
};

class timeslot_active_running : public timeslot {
      public:
        explicit timeslot_active_running(const double& pos_x, const double& pos_y,
                                         const double& duration)
            : timeslot(pos_x, pos_y, duration, "#55ff55ff"){};
};

class timeslot_active_non_cont : public timeslot {
      public:
        explicit timeslot_active_non_cont(const double& pos_x, const double& pos_y,
                                          const double& duration)
            : timeslot(pos_x, pos_y, duration, "#ffdd55ff"){};
};

#endif
