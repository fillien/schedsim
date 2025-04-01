#include <simulator/engine.hpp>
#include <simulator/server.hpp>
#include <simulator/task.hpp>

#include <cassert>
#include <memory>
#include <stdexcept>

Task::Task(std::weak_ptr<Engine> engine, std::size_t tid, double period, double utilization)
    : Entity(std::move(engine)), id_(tid), period_(period), utilization_(utilization)
{
}

auto Task::has_remaining_time() const noexcept -> bool
{
        return (Engine::round_zero(remaining_execution_time_) > 0);
}

auto Task::add_job(double duration) -> void
{
        assert(duration >= 0);
        if (pending_jobs_.empty() && Engine::round_zero(remaining_execution_time_) <= 0) {
                remaining_execution_time_ = duration;
        }
        else {
                pending_jobs_.push(duration);
        }
}

auto Task::remaining_time() const noexcept -> double
{
        const auto& FMAX_CHIP{sim()->chip()->clusters().at(0)->freq_max()};
        const auto& FMAX_CLUSTER{attached_proc_->cluster()->freq_max()};
        const auto& PERF{attached_proc_->cluster()->perf()};
        const auto& SPEED{attached_proc_->cluster()->speed()};
        return ((remaining_execution_time_ * (FMAX_CHIP / FMAX_CLUSTER)) / PERF) / SPEED;
}

auto Task::consume_time(double duration) -> void
{
        assert(duration >= 0);
        const auto& FMAX_CHIP{sim()->chip()->clusters().at(0)->freq_max()};
        const auto& FMAX_CLUSTER{attached_proc_->cluster()->freq_max()};
        const auto& PERF{attached_proc_->cluster()->perf()};
        const auto& SPEED{attached_proc_->cluster()->speed()};
        remaining_execution_time_ -= ((duration / (FMAX_CHIP / FMAX_CLUSTER)) * PERF) * SPEED;
        assert(sim()->round_zero(remaining_execution_time_) >= 0);
}

auto Task::next_job() -> void
{
        if (pending_jobs_.empty()) { throw std::runtime_error("no next job to execute"); }

        remaining_execution_time_ = pending_jobs_.front();
        pending_jobs_.pop();
}

auto Task::server(const std::shared_ptr<Server>& serv_to_attach) -> void
{
        attached_serv_ = serv_to_attach;
        serv_to_attach->attach_task(shared_from_this());
}

auto Task::clear_server() -> void { attached_serv_.reset(); }
