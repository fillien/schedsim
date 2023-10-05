#include "processor.hpp"
#include "engine.hpp"
#include "server.hpp"
#include "task.hpp"

#include <cassert>
#include <iostream>
#include <memory>

processor::processor(const std::weak_ptr<engine> sim, const int id) : entity(sim), id(id){};

void processor::set_server(std::weak_ptr<server> server_to_execute) {
        assert(!server_to_execute.expired());
        auto const& serv = server_to_execute.lock();

        if (running_server.expired()) {
                /// TODO change processor state management by writing a update_state method that
                /// check if there is a running server on the proc at the end of the resched phase.
                // change_state(state::running);
        }
        running_server = serv;
        serv->attached_task->attached_proc = shared_from_this();
        sim()->logging_system.add_trace({sim()->current_timestamp, types::TASK_SCHEDULED, id, 0});
}

void processor::clear_server() {
        std::cout << "clear server on proc " << id << std::endl;
        running_server.lock()->attached_task->attached_proc = nullptr;
        this->running_server.reset();
}

void processor::change_state(const processor::state next_state) {
        // assert that a processor can't enter twice in the same state
        if (next_state == current_state)
                return;

        switch (next_state) {
        case state::idle: {
                current_state = state::idle;
                sim()->logging_system.add_trace(
                    {sim()->current_timestamp, types::PROC_IDLED, id, 0});
                break;
        }
        case state::running: {
                current_state = state::running;
                sim()->logging_system.add_trace(
                    {sim()->current_timestamp, types::PROC_ACTIVATED, id, 0});
                break;
        }
        }
}

void processor::update_state() {
        if (has_server_running()) {
                change_state(state::running);
        } else {
                change_state(state::idle);
        }
}
