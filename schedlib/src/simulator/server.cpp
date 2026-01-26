#include <cassert>
#include <simulator/engine.hpp>
#include <simulator/event.hpp>
#include <simulator/scheduler.hpp>
#include <simulator/server.hpp>
#include <simulator/task.hpp>
#include <variant>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

Server::Server(Engine& sim, scheds::Scheduler* sched) : Entity(sim), attached_sched_(sched) {}

auto Server::running_time() const -> double { return sim().time() - last_update_; }

auto Server::update_time() -> void { last_update_ = sim().time(); }

auto Server::change_state(State new_state) -> void
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;

        // If the requested state is already set, exit early.
        if (new_state == current_state_) { return; }

        // Cache the current simulation time.
        const auto current_time = sim().time();

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
                        sim().add_trace(traces::ServReady{
                            .sched_id = scheduler()->cluster()->id(),
                            .task_id = id(),
                            .deadline = relative_deadline_,
                            .utilization = utilization()});
                        break;
                }
                case State::NonCont: {
                        // Remove all future ServInactive events for this server.
                        const auto serv_id = id();
                        sim().remove_event([serv_id](const auto& evt) -> bool {
                                if (const auto* res =
                                        std::get_if<events::ServInactive>(&evt.second)) {
                                        return res->serv->id() == serv_id;
                                }
                                return false;
                        });
                        cant_be_inactive_ = true;
                        sim().add_trace(traces::ServReady{
                            .sched_id = scheduler()->cluster()->id(),
                            .task_id = id(),
                            .deadline = relative_deadline_,
                            .utilization = utilization()});
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
                sim().add_trace(traces::ServRunning{
                    .sched_id = scheduler()->cluster()->id(), .task_id = id()});
                last_update_ = current_time;
                current_state_ = State::Running;
                break;
        }
        case State::NonCont: {
                // Transition from Running to NonCont.
                assert(current_state_ == State::Running);
                sim().add_trace(traces::ServNonCont{
                    .sched_id = scheduler()->cluster()->id(), .task_id = id()});
                // Virtual time must be in the future.
                assert(
                    virtual_time_ > current_time &&
                    "Virtual time must be greater than the current time.");
                sim().add_event(events::ServInactive{this}, virtual_time_);
                current_state_ = State::NonCont;
                break;
        }
        case State::Inactive: {
                // Valid only from Running or NonCont.
                assert(current_state_ == State::Running || current_state_ == State::NonCont);
                sim().add_trace(traces::ServInactive{
                    .sched_id = scheduler()->cluster()->id(),
                    .task_id = id(),
                    .utilization = utilization()});
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
        sim().add_trace(traces::ServPostpone{
            .sched_id = scheduler()->cluster()->id(),
            .task_id = id(),
            .deadline = relative_deadline_});
}
