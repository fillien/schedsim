#include "server.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "task.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <unordered_map>

server::server(const std::weak_ptr<engine>& sim, const std::weak_ptr<task>& attached_task)
    : entity(sim), attached_task(attached_task){};

auto server::get_budget() -> double {
        const double active_bw = sim()->sched->get_active_bandwidth();
        return utilization() / active_bw * (relative_deadline - virtual_time);
}

void server::change_state(const state& new_state) {
        using enum types;

        assert(new_state != current_state);

        if (last_call != sim()->current_timestamp) {
                last_call = sim()->current_timestamp;
                cant_be_inactive = false;
        }

        std::cout << current_state << " -> " << new_state << std::endl;

        switch (new_state) {
        case state::ready: {
                switch (current_state) {
                case state::inactive: {
                        // Job arrival
                        relative_deadline = sim()->current_timestamp + period();
                        sim()->logging_system.traceGotoReady(id());
                        break;
                }
                case state::non_cont: {
                        // Remove all future events of type SERV_INACTIVE
                        /// TODO Replace events insertion and deletion by a timer mechanism.
                        auto const& serv_id = id();
                        std::erase_if(sim()->future_list, [serv_id](const auto& event) {
                                auto const& evt_t = event.second.type;
                                if (evt_t == SERV_INACTIVE) {
                                        auto const& serv = std::static_pointer_cast<server>(
                                            event.second.target.lock());
                                        return serv->id() == serv_id;
                                }
                                return false;
                        });
                        cant_be_inactive = true;
                        sim()->logging_system.add_trace(
                            {sim()->current_timestamp, SERV_READY, id(), 0});
                        break;
                }
                case state::ready:
                case state::running: {
                        break;
                }
                default: assert(false);
                }
                current_state = state::ready;
                break;
        }
        case state::running: {
                assert(current_state == state::ready);
                // Dispatch
                sim()->logging_system.add_trace({sim()->current_timestamp, SERV_RUNNING, id(), 0});
                last_update = sim()->current_timestamp;
                current_state = state::running;
                break;
        }
        case state::non_cont: {
                assert(current_state == state::running);
                sim()->logging_system.add_trace({sim()->current_timestamp, SERV_NON_CONT, id(), 0});

                // Insert a event to pass in IDLE state when the time will be equal to the
                // virtual time. Deleting this event is necessery if a job arrive.
                assert(virtual_time > sim()->current_timestamp);
                sim()->add_event({SERV_INACTIVE, shared_from_this(), 0}, virtual_time);
                current_state = state::non_cont;
                break;
        }
        case state::inactive: {
                assert(current_state == state::running || current_state == state::non_cont);
                sim()->logging_system.add_trace({sim()->current_timestamp, SERV_INACTIVE, id(), 0});
                current_state = state::inactive;
                break;
        }
        }
}

void server::postpone() {
        relative_deadline += period();
        sim()->logging_system.add_trace({sim()->current_timestamp, types::SERV_POSTPONE, id(), 0});
}

auto operator<<(std::ostream& out, const server& serv) -> std::ostream& {
        return out << "S" << serv.id() << " P=" << serv.period() << " U=" << serv.utilization()
                   << " D=" << serv.relative_deadline << " V=" << serv.virtual_time;
}

auto operator<<(std::ostream& out, const server::state& serv_state) -> std::ostream& {
        using enum server::state;
        switch (serv_state) {
        case inactive: return out << "inactive";
        case ready: return out << "ready";
        case running: return out << "running";
        case non_cont: return out << "non_cont";
        default: return out << "unknown";
        }
}
