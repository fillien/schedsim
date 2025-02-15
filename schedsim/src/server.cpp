#include <cassert>
#include <engine.hpp>
#include <entity.hpp>
#include <event.hpp>
#include <iostream>
#include <memory>
#include <server.hpp>
#include <task.hpp>
#include <variant>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

Server::Server(const std::weak_ptr<Engine>& sim) : Entity(sim) {};

void Server::set_task(const std::shared_ptr<Task>& task_to_attach)
{
        attached_task = task_to_attach;
}

void Server::unset_task() { attached_task.reset(); }

auto Server::remaining_exec_time() const -> double { return get_task()->remaining_time(); }

void Server::change_state(const state& new_state)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        assert(new_state != current_state);

        if (last_call != sim()->time()) {
                last_call = sim()->time();
                cant_be_inactive = false;
        }

        switch (new_state) {
        case state::ready: {
                switch (current_state) {
                case state::inactive: {
                        // Job arrival
                        relative_deadline = sim()->time() + period();
                        sim()->add_trace(traces::ServReady{
                            shared_from_this()->id(), relative_deadline, this->utilization()});
                        break;
                }
                case state::non_cont: {
                        // Remove all future events of type SERV_INACTIVE
                        /// TODO Replace events insertion and deletion by a timer mechanism.
                        auto const& serv_id = id();
                        sim()->remove_event([serv_id](const auto& evt) {
                                if (const auto& res =
                                        std::get_if<events::ServInactive>(&evt.second)) {
                                        return res->serv->id() == serv_id;
                                }
                                return false;
                        });
                        cant_be_inactive = true;
                        sim()->add_trace(traces::ServReady{
                            shared_from_this()->id(), relative_deadline, this->utilization()});
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
                assert(current_state == state::ready || current_state == state::running);
                // Dispatch
                sim()->add_trace(traces::ServRunning{shared_from_this()->id()});
                last_update = sim()->time();
                current_state = state::running;
                break;
        }
        case state::non_cont: {
                assert(current_state == state::running);
                sim()->add_trace(traces::ServNonCont{shared_from_this()->id()});

                // Insert a event to pass in IDLE state when the time will be equal to the
                // virtual time. Deleting this event is necessery if a job arrive.
                assert(virtual_time > sim()->time());
                sim()->add_event(events::ServInactive{shared_from_this()}, virtual_time);
                current_state = state::non_cont;
                break;
        }
        case state::inactive: {
                assert(current_state == state::running || current_state == state::non_cont);
                sim()->add_trace(traces::ServInactive{shared_from_this()->id(), utilization()});
                current_state = state::inactive;
                break;
        }
        }
}

void Server::postpone()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        relative_deadline += period();
        sim()->add_trace(traces::ServPostpone{shared_from_this()->id(), relative_deadline});
}

auto operator<<(std::ostream& out, const Server& serv) -> std::ostream&
{
        return out << "S" << serv.id() << " P=" << serv.period() << " U=" << serv.utilization()
                   << " D=" << serv.relative_deadline << " V=" << serv.virtual_time;
}

auto operator<<(std::ostream& out, const Server::state& serv_state) -> std::ostream&
{
        using enum Server::state;
        switch (serv_state) {
        case inactive: return out << "inactive";
        case ready: return out << "ready";
        case running: return out << "running";
        case non_cont: return out << "non_cont";
        default: return out << "unknown";
        }
}
