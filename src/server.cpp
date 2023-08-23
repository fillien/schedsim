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

auto server::get_budget() -> double {
        const double active_bw = sim()->sched->get_active_bandwidth();
        return utilization() / active_bw * (relative_deadline - virtual_time);
}

void server::change_state(const state& new_state) {
        switch (new_state) {
        case state::inactive: std::cout << "inactive"; break;
        case state::non_cont: std::cout << "non_cont"; break;
        case state::ready: std::cout << "ready"; break;
        case state::running: std::cout << "running"; break;
        }
        std::cout << std::endl;
        assert(new_state != current_state);

        switch (new_state) {
        case state::ready: {
                switch (current_state) {
                case state::inactive: {
                        // Job arrival
                        relative_deadline = sim()->current_timestamp + period();
                        sim()->logging_system.traceGotoReady(id());
                        break;
                }
                case state::ready:
                case state::running: {
                        // Preempted
                        sim()->logging_system.add_trace(
                            {sim()->current_timestamp, types::TASK_PREEMPTED, id(), 0});
                        break;
                }
                default: assert(false);
                }
                current_state = state::ready;
                break;
        }
        case state::running: {
                switch (current_state) {
                case state::ready: {
                        // Dispatch
                        sim()->logging_system.add_trace(
                            {sim()->current_timestamp, types::SERV_RUNNING, id(), 0});
                        sim()->logging_system.add_trace(
                            {sim()->current_timestamp, types::TASK_SCHEDULED, id(), 0});
                        break;
                }
                default: assert(false);
                }
                current_state = state::running;
                break;
        }
        case state::non_cont: {
                switch (current_state) {
                case state::running: {
                        sim()->logging_system.add_trace(
                            {sim()->current_timestamp, types::SERV_NON_CONT, id(), 0});

                        // Insert a event to pass in IDLE state when the time will be equal to the
                        // virtual time. Deleting this event is necessery if a job arrive.
                        sim()->add_event({types::SERV_INACTIVE, shared_from_this(), 0},
                                         virtual_time);
                        break;
                }
                default: assert(false);
                }
                current_state = state::non_cont;
                break;
        }
        case state::inactive: {
                switch (current_state) {
                case state::running: {
                        // Anticipated end
                        break;
                }
                case state::non_cont: {
                        // Ending
                        break;
                }
                default: assert(false);
                }
                break;
        }
        }
}

void server::update_times(const double& timeframe) {
        const double active_bw = sim()->sched->get_active_bandwidth();
        virtual_time += timeframe * (active_bw / utilization());
        sim()->logging_system.add_trace(
            {sim()->current_timestamp, types::VIRTUAL_TIME_UPDATE, id(), virtual_time});
        attached_task.lock()->remaining_execution_time -= timeframe;
}

void server::postpone() {
        relative_deadline += period();
        sim()->logging_system.add_trace({sim()->current_timestamp, types::SERV_POSTPONE, id(), 0});
}

auto operator<<(std::ostream& out, const server& serv) -> std::ostream& {
        return out << "S" << serv.id() << " P=" << serv.period() << " U=" << serv.utilization()
                   << " D=" << serv.relative_deadline << " V=" << serv.virtual_time;
}
