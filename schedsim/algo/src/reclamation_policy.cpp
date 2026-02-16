#include <schedsim/algo/reclamation_policy.hpp>

#include <schedsim/algo/cbs_server.hpp>

namespace schedsim::algo {

core::TimePoint ReclamationPolicy::compute_virtual_time(
    const CbsServer& server, core::TimePoint current_vt, core::Duration exec_time) const {
    // Default CBS formula: vt += exec_time / U_server
    core::Duration vt_increment = core::divide_duration(exec_time, server.utilization());
    return current_vt + vt_increment;
}

core::Duration ReclamationPolicy::compute_server_budget(const CbsServer& server) const {
    // Default: CBS static budget
    return server.remaining_budget();
}

} // namespace schedsim::algo
