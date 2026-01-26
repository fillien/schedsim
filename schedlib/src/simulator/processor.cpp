#include <protocols/traces.hpp>
#include <simulator/engine.hpp>
#include <simulator/platform.hpp>
#include <simulator/processor.hpp>
#include <simulator/server.hpp>
#include <simulator/task.hpp>

#include <cassert>
#include <memory>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

Processor::Processor(Engine& sim, Cluster* cluster, std::size_t cpu_id)
    : Entity(sim), id_(cpu_id), cluster_(cluster)
{
        using namespace protocols::traces;

        sim.add_trace(ProcIdled{.proc_id = id_, .cluster_id = cluster_->id()});

        core_timer_ = std::make_unique<Timer>(sim, [this]() {
                assert(current_state_ == State::Change);
                change_state(dpm_target_);

                if (auto* scheduler = cluster_->scheduler()) {
                        this->sim().alloc().call_resched(scheduler);
                }

                assert(current_state_ != State::Change);
        });
}

void Processor::task(Task* task_to_execute)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        assert(task_to_execute != nullptr);

        // Set bidirectional relationship
        task_ = task_to_execute;
        task_to_execute->proc(this);

        sim().add_trace(
            protocols::traces::TaskScheduled{.task_id = task_to_execute->id(), .proc_id = id_});
}

void Processor::clear_task()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (task_) { task_->proc(nullptr); }
        task_ = nullptr;
}

void Processor::change_state(const State& next_state)
{
        using namespace protocols::traces;
        using enum State;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (next_state == current_state_) { return; }

        current_state_ = next_state;
        switch (next_state) {
        case Idle: sim().add_trace(ProcIdled{id_, cluster()->id()}); break;
        case Running: sim().add_trace(ProcActivated{id_, cluster()->id()}); break;
        case Sleep: sim().add_trace(ProcSleep{id_, cluster()->id()}); break;
        case Change:
                assert(!has_task());
                sim().add_trace(ProcChange{id_, cluster()->id()});
                break;
        }
}

void Processor::dvfs_change_state(double delay)
{
        if (!sim().is_delay_activated()) { return; }

        if (current_state_ == State::Change) {
                if (core_timer_->deadline() < (sim().time() + delay)) {
                        core_timer_->cancel();
                        dpm_target_ = State::Idle;
                        core_timer_->set(delay);
                }
        }
        else {
                change_state(State::Change);
                dpm_target_ = State::Idle;
                core_timer_->set(delay);
        }
}

void Processor::dpm_change_state(const State& next_state)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (next_state == current_state_) { return; }

        if (!sim().is_delay_activated()) {
                change_state(next_state);
                return;
        }

        if (current_state_ == State::Change) {
                if (core_timer_->deadline() < (sim().time() + DPM_DELAY)) {
                        core_timer_->cancel();
                        dpm_target_ = next_state;
                        core_timer_->set(DPM_DELAY);
                }
        }
        else {
                change_state(State::Change);
                dpm_target_ = next_state;
                core_timer_->set(DPM_DELAY);
        }
}
