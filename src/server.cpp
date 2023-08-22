#include "server.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "task.hpp"

#include <cassert>
#include <iostream>
#include <memory>

server::server(const std::weak_ptr<engine> sim, const std::weak_ptr<task> attached_task)
    : entity(sim), attached_task(attached_task){};

auto server::get_budget(const double& active_bw) -> double {
        return utilization() / active_bw * (relative_deadline - virtual_time);
}

void server::change_state(const state& new_state) {
        switch (new_state) {
        case state::ready: {
                switch (current_state) {
                case state::inactive: {
                        // Job arrival

                        return;
                }
                case state::running: {
                        // Preempted
                        return;
                }
                default: assert(false);
                }
        }
        case state::running: {
                switch (current_state) {
                case state::ready: {
                        // Dispatch
                        return;
                }
                default: assert(false);
                }
        }
        case state::non_cont: {
                switch (current_state) {
                case state::running: {
                        current_state = state::non_cont;
                        sim()->logging_system.add_trace(
                            {sim()->current_timestamp, types::SERV_NON_CONT, id(), 0});

                        // Insert a event to pass in IDLE state when the time will be equal to the
                        // virtual time. Deleting this event is necessery if a job arrive.
                        sim()->add_event({types::SERV_INACTIVE, shared_from_this(), 0},
                                         virtual_time);
                        return;
                }
                default: assert(false);
                }
        }
        case state::inactive: {
                switch (current_state) {
                case state::running: {
                        // Anticipated end
                        return;
                }
                case state::non_cont: {
                        // Ending
                        return;
                }
                default: assert(false);
                }
        }
        }
}

auto operator<<(std::ostream& out, const server& serv) -> std::ostream& {
        return out << "S" << serv.id() << " P=" << serv.period() << " U=" << serv.utilization()
                   << " D=" << serv.relative_deadline << " V=" << serv.virtual_time;
}
