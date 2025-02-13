#include <cassert>
#include <engine.hpp>
#include <iostream>
#include <memory>
#include <platform.hpp>
#include <processor.hpp>
#include <protocols/traces.hpp>
#include <server.hpp>
#include <task.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

Processor::Processor(
    const std::weak_ptr<Engine>& sim, const std::weak_ptr<Cluster>& clu, const std::size_t cpu_id)
    : Entity(sim), id(cpu_id), attached_cluster(clu)
{
        using namespace protocols::traces;

        switch (current_state) {
        case state::idle: {
                sim.lock()->add_trace(ProcIdled{cpu_id});
                break;
        }
        case state::running: {
                sim.lock()->add_trace(ProcActivated{cpu_id});
                break;
        }
        case state::sleep: {
                sim.lock()->add_trace(ProcSleep{cpu_id});
                break;
        }
        case state::change: {
                sim.lock()->add_trace(ProcChange{cpu_id});
        }
        }

        coretimer = std::make_shared<Timer>(sim, [this, sim]() {
                assert(this->current_state == state::change);
                this->change_state(dpm_target);
                sim.lock()->get_sched()->call_resched(attached_cluster.lock()->get_sched().lock());
                assert(this->current_state != state::change);
        });
};

void Processor::set_task(const std::weak_ptr<Task>& task_to_execute)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        assert(!task_to_execute.expired());

        auto shared_task = task_to_execute.lock();

        // Set bidiretionnal relationship
        running_task = std::move(shared_task);
        shared_task->attached_proc = shared_from_this();

        sim()->add_trace(
            protocols::traces::TaskScheduled{shared_task->id, shared_from_this()->id});
}

void Processor::clear_task()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        assert(!running_task.expired());
        running_task.lock()->attached_proc = nullptr;
        this->running_task.reset();
}

void Processor::change_state(const Processor::state& next_state)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;

        if (next_state == current_state) { return; }

        switch (next_state) {
        case state::idle: {
                current_state = state::idle;
                sim()->add_trace(traces::ProcIdled{shared_from_this()->id});
                break;
        }
        case state::running: {
                current_state = state::running;
                sim()->add_trace(traces::ProcActivated{shared_from_this()->id});
                break;
        }
        case state::sleep: {
                current_state = state::sleep;
                sim()->add_trace(traces::ProcSleep{shared_from_this()->id});
                break;
        }
        case state::change: {
                assert(has_running_task() == false);
                current_state = state::change;
                sim()->add_trace(traces::ProcChange{shared_from_this()->id});
        }
        }
}

void Processor::update_state()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (has_running_task()) { change_state(state::running); }
        else {
                change_state(state::idle);
        }
}

void Processor::dvfs_change_state(const double& delay)
{
        assert(sim()->is_delay_active());

        if (current_state == state::change) {
                if (coretimer->get_deadline() < (sim()->time() + delay)) {
                        coretimer->cancel();
                        dpm_target = state::idle;
                        coretimer->set(delay);
                }
        }
        else {
                change_state(state::change);
                dpm_target = state::idle;
                coretimer->set(delay);
        }
}

void Processor::dpm_change_state(const state& next_state)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        assert(next_state != current_state);

        if (!sim()->is_delay_active()) {
                change_state(next_state);
                return;
        }

        if (current_state == state::change) {
                if (coretimer->get_deadline() < (sim()->time() + DPM_DELAY)) {
                        coretimer->cancel();
                        dpm_target = next_state;
                        coretimer->set(DPM_DELAY);
                }
        }
        else {
                change_state(state::change);
                dpm_target = next_state;
                coretimer->set(DPM_DELAY);
        }
}
