#include "processor.hpp"
#include "engine.hpp"
#include "server.hpp"
#include "task.hpp"
#include <platform.hpp>
#include <protocols/traces.hpp>

#include <cassert>
#include <iostream>
#include <memory>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

processor::processor(
    const std::weak_ptr<engine>& sim, const std::weak_ptr<cluster>& clu, const std::size_t cpu_id)
    : entity(sim), id(cpu_id), attached_cluster(clu)
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
        case state::change: {
                sim.lock()->add_trace(proc_change{cpu_id});
        }
        }

        coretimer = std::make_shared<timer>(sim, [this, sim]() {
                assert(this->current_state == state::change);
                this->change_state(dpm_target);
                sim.lock()->get_sched()->call_resched(attached_cluster.lock()->get_sched().lock());
                assert(this->current_state != state::change);
        });
};

void processor::set_task(const std::weak_ptr<task>& task_to_execute)
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
            protocols::traces::task_scheduled{shared_task->id, shared_from_this()->id});
}

void processor::clear_task()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        assert(!running_task.expired());
        running_task.lock()->attached_proc = nullptr;
        this->running_task.reset();
}

void processor::change_state(const processor::state& next_state)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        namespace traces = protocols::traces;

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
        case state::change: {
                assert(has_running_task() == false);
                current_state = state::change;
                sim()->add_trace(traces::proc_change{shared_from_this()->id});
        }
        }
}

void processor::update_state()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (has_running_task()) { change_state(state::running); }
        else {
                change_state(state::idle);
        }
}

void processor::dvfs_change_state(const double& delay)
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

void processor::dpm_change_state(const state& next_state)
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
