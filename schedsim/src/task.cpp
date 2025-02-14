#include <cassert>
#include <engine.hpp>
#include <memory>
#include <server.hpp>
#include <task.hpp>

Task::Task(
    const std::weak_ptr<Engine>& sim,
    const std::size_t tid,
    const double& period,
    const double& utilization)
    : Entity(sim), id(tid), period(period), utilization(utilization)
{
}

auto Task::is_attached() const -> bool { return (attached_proc != nullptr); }

auto Task::has_server() const -> bool { return (attached_serv.use_count() > 0); };

void Task::set_server(const std::shared_ptr<Server>& serv_to_attach)
{
        attached_serv = serv_to_attach;
        serv_to_attach->set_task(shared_from_this());
}

void Task::unset_server() { attached_serv.reset(); }

auto Task::has_remaining_time() const -> bool
{
        return (sim()->round_zero(this->remaining_execution_time) > 0);
}

void Task::add_job(const double& duration)
{
        assert(duration >= 0);
        if (pending_jobs.empty() && sim()->round_zero(remaining_execution_time) <= 0) {
                remaining_execution_time = duration;
        }
        else {
                pending_jobs.push(duration);
        }
}

auto Task::get_remaining_time() const -> double
{
        return remaining_execution_time / attached_proc->cluster()->speed();
};

void Task::consume_time(const double& duration)
{
        assert(duration >= 0);

        remaining_execution_time -= duration * attached_proc->cluster()->speed();
        assert(sim()->round_zero(remaining_execution_time) >= 0);
}

void Task::next_job()
{
        remaining_execution_time = pending_jobs.front();
        pending_jobs.pop();
}
