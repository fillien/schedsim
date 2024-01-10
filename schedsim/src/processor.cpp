#include "processor.hpp"
#include "engine.hpp"
#include "server.hpp"
#include "task.hpp"
#include <protocols/traces.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>

processor::processor(const std::weak_ptr<engine>& sim, int cpu_id) : entity(sim), id(cpu_id){};

void processor::set_server(std::weak_ptr<server> server_to_execute)
{
        namespace traces = protocols::traces;
        assert(!server_to_execute.expired());
        auto const& serv = server_to_execute.lock();

        running_server = serv;
        serv->get_task()->attached_proc = shared_from_this();
        sim()->add_trace(traces::task_scheduled{serv->get_task()->id, shared_from_this()->id});
}

void processor::clear_server()
{
        running_server.lock()->get_task()->attached_proc = nullptr;
        this->running_server.reset();
}

void processor::change_state(const processor::state& next_state)
{
        namespace traces = protocols::traces;
        // assert that a processor can't enter twice in the same state
        if (next_state == current_state) { return; }

        switch (next_state) {
        case state::idle: {
                current_state = state::idle;
                sim()->add_trace(traces::proc_idled{shared_from_this()->id});
                break;
        }
        case state::running: {
                current_state = state::running;
                sim()->add_trace(traces::proc_activated{shared_from_this()->id});
                break;
        }
        }
}

void processor::update_state()
{
        if (has_server_running()) { change_state(state::running); }
        else {
                change_state(state::idle);
        }
}
