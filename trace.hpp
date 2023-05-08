#ifndef TRACE_HPP
#define TRACE_HPP

#include "entity.hpp"
#include "server.hpp"

#include <memory>
#include <ostream>
#include <string_view>
#include <vector>

class trace {
      public:
        enum class types {
                tbegin,
                tend,
                tpostp_b,
                tarrival,
                tblocked,
                tpreempted,
                tmigrate,
                tdeadlmiss,
                pfreq_change,
                pcstate_change,
                pstopped,
                pidled,
                pactivated,
                pstopping,
                penergy,
                sactcont,
                sactnoncont,
                sinact,
                sbudgetex,
                sbudgetrep,
                sdlpostpone,
                svirtt,
                sdeadl
        };

        const double timestamp;
        const types event;

        trace(const double& timestamp, const types event);
};

auto operator<<(std::ostream& out, const trace::types& trace_ev) -> std::ostream&;
auto to_txt(const trace& trace) -> std::string;
auto to_txt_fanch(const trace& trace) -> std::string;
auto to_csv(const trace& trace) -> std::string;

#endif
