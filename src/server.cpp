#include "server.hpp"
#include "engine.hpp"
#include "entity.hpp"
#include "event.hpp"
#include "task.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <unordered_map>
#include <variant>

server::server(const std::weak_ptr<engine>& sim, const std::weak_ptr<task>& attached_task)
    : entity(sim), attached_task(attached_task){};

void server::change_state(const state& new_state) {
        assert(new_state != current_state);

        if (last_call != sim()->get_time()) {
                last_call = sim()->get_time();
                cant_be_inactive = false;
        }

        std::cout << current_state << " -> " << new_state << std::endl;

        switch (new_state) {
        case state::ready: {
                switch (current_state) {
                case state::inactive: {
                        // Job arrival
                        relative_deadline = sim()->get_time() + period();
                        sim()->add_trace(events::serv_ready{shared_from_this()});
                        break;
                }
                case state::non_cont: {
                        // Remove all future events of type SERV_INACTIVE
                        /// TODO Replace events insertion and deletion by a timer mechanism.
                        auto const& serv_id = id();
                        std::erase_if(sim()->future_list, [serv_id](const auto& evt) {
                                if (holds_alternative<events::serv_inactive>(evt.second)) {
                                        auto knowned = std::get<events::serv_inactive>(evt.second);
                                        return knowned.serv->id() == serv_id;
                                }
                                return false;
                        });
                        cant_be_inactive = true;
                        sim()->add_trace(events::serv_ready{shared_from_this()});
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
                sim()->add_trace(events::serv_running{shared_from_this()});
                last_update = sim()->get_time();
                current_state = state::running;
                break;
        }
        case state::non_cont: {
                assert(current_state == state::running);
                sim()->add_trace(events::serv_non_cont{shared_from_this()});

                // Insert a event to pass in IDLE state when the time will be equal to the
                // virtual time. Deleting this event is necessery if a job arrive.
                assert(virtual_time > sim()->get_time());
                sim()->add_event(events::serv_inactive{shared_from_this()}, virtual_time);
                current_state = state::non_cont;
                break;
        }
        case state::inactive: {
                assert(current_state == state::running || current_state == state::non_cont);
                sim()->add_trace(events::serv_inactive{shared_from_this()});
                current_state = state::inactive;
                break;
        }
        }
}

void server::postpone() {
        relative_deadline += period();
        sim()->add_trace(events::serv_postpone{shared_from_this(), relative_deadline});
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
