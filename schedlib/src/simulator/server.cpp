#include <cassert>
#include <simulator/engine.hpp>
#include <simulator/event.hpp>
#include <memory>
#include <simulator/scheduler.hpp>
#include <simulator/server.hpp>
#include <simulator/task.hpp>
#include <variant>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

Server::Server(const std::weak_ptr<Engine>& sim, const std::shared_ptr<scheds::Scheduler>& sched)
    : Entity(sim), attached_sched_(sched)
{
}

auto Server::running_time() const -> double { return sim()->time() - last_update_; }

auto Server::update_time() -> void { last_update_ = sim()->time(); }

auto Server::change_state(State new_state) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;

        // If the requested state is already set, exit early.
        if (new_state == current_state_) { return; }

        // Cache the current simulation time and a shared pointer to self.
        const auto current_time = sim()->time();
        auto self = shared_from_this();

        // Reset the inactive flag if time has advanced.
        if (last_call_ != current_time) {
                last_call_ = current_time;
                cant_be_inactive_ = false;
        }

        switch (new_state) {
        case State::Ready: {
                switch (current_state_) {
                case State::Inactive: {
                        // Job arrival: compute new relative deadline.
                        relative_deadline_ = current_time + period();
                        sim()->add_trace(traces::ServReady{
                            .task_id = self->id(),
                            .deadline = relative_deadline_,
                            .utilization = utilization() / scheduler()->cluster()->perf()});
                        break;
                }
                case State::NonCont: {
                        // Remove all future ServInactive events for this server.
                        const auto serv_id = id();
                        sim()->remove_event([serv_id](const auto& evt) -> bool {
                                if (const auto* res =
                                        std::get_if<events::ServInactive>(&evt.second)) {
                                        return res->serv->id() == serv_id;
                                }
                                return false;
                        });
                        cant_be_inactive_ = true;
                        sim()->add_trace(traces::ServReady{
                            .task_id = self->id(),
                            .deadline = relative_deadline_,
                            .utilization = utilization() / scheduler()->cluster()->perf()});
                        break;
                }
                case State::Ready:
                case State::Running:
                        // No additional action needed.
                        break;
                default: assert(false && "Invalid current state transition to Ready");
                }
                current_state_ = State::Ready;
                break;
        }
        case State::Running: {
                // Valid only if currently Ready or already Running.
                assert(current_state_ == State::Ready || current_state_ == State::Running);
                sim()->add_trace(traces::ServRunning{self->id()});
                last_update_ = current_time;
                current_state_ = State::Running;
                break;
        }
        case State::NonCont: {
                // Transition from Running to NonCont.
                assert(current_state_ == State::Running);
                sim()->add_trace(traces::ServNonCont{self->id()});
                // Virtual time must be in the future.
                assert(
                    virtual_time_ > current_time &&
                    "Virtual time must be greater than the current time.");
                sim()->add_event(events::ServInactive{self}, virtual_time_);
                current_state_ = State::NonCont;
                break;
        }
        case State::Inactive: {
                // Valid only from Running or NonCont.
                assert(current_state_ == State::Running || current_state_ == State::NonCont);
                sim()->add_trace(traces::ServInactive{
                    .task_id = self->id(),
                    .utilization = utilization() / scheduler()->cluster()->perf()});
                current_state_ = State::Inactive;
                break;
        }
        }
}

auto Server::postpone() -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        // Postpone the server by updating the relative deadline.
        relative_deadline_ += period();
        sim()->add_trace(traces::ServPostpone{
            .task_id = shared_from_this()->id(), .deadline = relative_deadline_});
}
