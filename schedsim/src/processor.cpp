#include "processor.hpp"
#include "engine.hpp"
#include "server.hpp"
#include "task.hpp"
#include <protocols/traces.hpp>

#include <cassert>
#include <memory>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

processor::processor(const std::weak_ptr<engine>& sim, std::size_t cpu_id) : entity(sim), id(cpu_id)
{
        using namespace protocols::traces;
        switch (current_state) {
        case state::idle: {
                sim.lock()->add_trace(proc_idled{cpu_id});
                break;
        }
        case state::running: {
                sim.lock()->add_trace(proc_activated{cpu_id});
                break;
        }
        case state::sleep: {
                sim.lock()->add_trace(proc_sleep{cpu_id});
                break;
        }
        }
};

void processor::set_server(std::weak_ptr<server> server_to_execute)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;
        assert(!server_to_execute.expired());
        auto const& serv = server_to_execute.lock();

        running_server = serv;
        serv->get_task()->attached_proc = shared_from_this();
        sim()->add_trace(traces::task_scheduled{serv->get_task()->id, shared_from_this()->id});
}

void processor::clear_server()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        running_server.lock()->get_task()->attached_proc = nullptr;
        this->running_server.reset();
}

void processor::change_state(const processor::state& next_state)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
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
        case state::sleep: {
                current_state = state::sleep;
                sim()->add_trace(traces::proc_sleep{shared_from_this()->id});
                break;
        }
        }
}

void processor::update_state()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (has_server_running()) { change_state(state::running); }
        else {
                change_state(state::idle);
        }
}
