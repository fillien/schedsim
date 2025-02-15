#include <cassert>
#include <engine.hpp>
#include <memory>
#include <platform.hpp>
#include <processor.hpp>
#include <protocols/traces.hpp>
#include <server.hpp>
#include <task.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

Processor::Processor(std::weak_ptr<Engine> sim, std::weak_ptr<Cluster> clu, std::size_t cpu_id)
    : Entity(sim), id_(cpu_id), cluster_(std::move(clu))
{
        using namespace protocols::traces;

        assert(sim.lock());
        sim.lock()->add_trace(ProcIdled{id_});

        core_timer_ = std::make_shared<Timer>(sim, [this, sim]() {
                assert(current_state_ == State::Change);
                change_state(dpm_target_);

                if (auto clu = cluster_.lock()) {
                        if (auto scheduler = clu->get_sched().lock()) {
                                sim.lock()->sched()->call_resched(scheduler);
                        }
                }

                assert(current_state_ != State::Change);
        });
}

void Processor::task(std::weak_ptr<Task> task_to_execute)
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        assert(!task_to_execute.expired());

        auto shared_task = task_to_execute.lock();

        // Set bidiretionnal relationship
        task_ = std::move(shared_task);
        shared_task->proc(shared_from_this());

        sim()->add_trace(protocols::traces::TaskScheduled{
            .task_id = shared_task->id(), .proc_id = shared_from_this()->id_});
}

void Processor::clear_task()
{
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (auto shared_task = task_.lock()) { shared_task->proc().reset(); }
        task_.reset();
}

auto Processor::task() const -> std::shared_ptr<Task> { return task_.lock(); }

void Processor::change_state(const State& next_state)
{
        using namespace protocols::traces;
        using enum State;
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        if (next_state == current_state_) { return; }

        if (auto engine = sim()) {
                current_state_ = next_state;
                switch (next_state) {
                case Idle: engine->add_trace(ProcIdled{id_}); break;
                case Running: engine->add_trace(ProcActivated{id_}); break;
                case Sleep: engine->add_trace(ProcSleep{id_}); break;
                case Change:
                        assert(!has_task());
                        engine->add_trace(ProcChange{id_});
                        break;
                }
        }
}

void Processor::dvfs_change_state(double delay)
{
        auto engine = sim();
        if (!engine || !engine->is_delay_activated()) { return; }

        if (current_state_ == State::Change) {
                if (core_timer_->deadline() < (engine->time() + delay)) {
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

        auto engine = sim();
        if (!engine || !engine->is_delay_activated()) {
                change_state(next_state);
                return;
        }

        if (current_state_ == State::Change) {
                if (core_timer_->deadline() < (engine->time() + DPM_DELAY)) {
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
