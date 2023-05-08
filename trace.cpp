#include "trace.hpp"

#include <array>
#include <cassert>
#include <iomanip>
#include <string>

trace::trace(const double& timestamp, const types event) : timestamp(timestamp), event(event) {}

auto operator<<(std::ostream& out, const trace::types& trace_ev) -> std::ostream& {
        using enum trace::types;
        switch (trace_ev) {
        case penergy: return out << "penergy";
        case pactivated: return out << "pactivated";
        case pcstate_change: return out << "pcstate_change";
        case pfreq_change: return out << "pfreq_change";
        case pidled: return out << "pideled";
        case pstopped: return out << "pstopped";
        case pstopping: return out << "pstopping";
        case sactcont: return out << "sactcont";
        case sactnoncont: return out << "sactnoncont";
        case sbudgetex: return out << "sbudgetex";
        case sbudgetrep: return out << "sbudgetrep";
        case sdeadl: return out << "sdeadl";
        case sdlpostpone: return out << "sdlpostpone";
        case sinact: return out << "sinact";
        case svirtt: return out << "svirtt";
        case tarrival: return out << "tarrival";
        case tbegin: return out << "tbegin";
        case tblocked: return out << "tblocked";
        case tdeadlmiss: return out << "tdeadlmiss";
        case tend: return out << "tend";
        case tmigrate: return out << "tmigrate";
        case tpostp_b: return out << "tpostp_b";
        case tpreempted: return out << "tpreempted";
        default: assert("Unknown trace event"); return out;
        }
}

auto to_txt(const trace& log) -> std::string {
        using namespace std;
        constexpr static array COLUMNS_SIZE{10, 10, 10};
        ostringstream out{};

        /// @todo check entity do print the right log message
        // string tstr = (log.task != nullptr ? to_string(log.task->_id()) : "E");
        // string pstr = (log.proc != nullptr ? to_string(log.proc->id) : "E");

        out << "Trace [event: " << left << setw(COLUMNS_SIZE[0]) << log.event << "] -> t: " << right
            << setw(COLUMNS_SIZE[1]) << log.timestamp << " | value: " << left
            << setw(COLUMNS_SIZE[2]) << '\n';

        return out.str();
};

auto to_txt_fanch(const trace& log) -> std::string {
        using namespace std;
        constexpr static array COLUMNS_SIZE{7, 10};
        ostringstream out{};

        out << "t:" << right << setw(COLUMNS_SIZE[0]) << log.timestamp << " | event: " << left
            << setw(COLUMNS_SIZE[1]) << log.event << '\n';

        return out.str();
}

auto to_csv(const trace& log) -> std::string {
        std::ostringstream out{};

        return out.str();
};
