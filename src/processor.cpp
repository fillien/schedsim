#include "processor.hpp"
#include "engine.hpp"
#include "event.hpp"
#include "server.hpp"
#include "task.hpp"

#include <cassert>
#include <iostream>
#include <memory>

processor::processor(const std::weak_ptr<engine>& sim, int cpu_id) : entity(sim), id(cpu_id){};

void processor::set_server(std::weak_ptr<server> server_to_execute) {
        assert(!server_to_execute.expired());
        auto const& serv = server_to_execute.lock();

        running_server = serv;
        serv->attached_task->attached_proc = shared_from_this();
        sim()->add_trace(events::task_scheduled{serv->attached_task, shared_from_this()});
}

void processor::clear_server() {
        std::cout << "clear server on proc " << id << std::endl;
        running_server.lock()->attached_task->attached_proc = nullptr;
        this->running_server.reset();
}

void processor::change_state(const processor::state& next_state) {
        // assert that a processor can't enter twice in the same state
        if (next_state == current_state) {
                return;
        }

        switch (next_state) {
        case state::idle: {
                current_state = state::idle;
                sim()->add_trace(events::proc_idled{shared_from_this()});
                break;
        }
        case state::running: {
                current_state = state::running;
                sim()->add_trace(events::proc_activated{shared_from_this()});
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
