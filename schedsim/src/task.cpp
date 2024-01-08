#include "task.hpp"
#include "engine.hpp"
#include "server.hpp"

#include <cassert>
#include <iostream>
#include <limits>
#include <memory>

task::task(
    const std::weak_ptr<engine>& sim,
    const std::size_t tid,
    const double& period,
    const double& utilization)
    : entity(sim), id(tid), period(period), utilization(utilization)
{
}

auto task::is_attached() const -> bool { return (attached_proc != nullptr); }

auto task::has_server() const -> bool { return (attached_serv.use_count() > 0); };

void task::set_server(const std::shared_ptr<server>& serv_to_attach)
{
        attached_serv = serv_to_attach;
        serv_to_attach->set_task(shared_from_this());
}

void task::unset_server() { attached_serv.reset(); }

auto task::has_remaining_time() const -> bool
{
        return (sim()->round_zero(this->remaining_execution_time) > 0);
}

void task::add_job(const double& duration)
{
        assert(duration >= 0);
        if (pending_jobs.empty() && sim()->round_zero(remaining_execution_time) <= 0) {
                remaining_execution_time = duration;
        }
        else {
                pending_jobs.push(duration);
        }
}

void task::consume_time(const double& duration)
{
        assert(duration >= 0);
        remaining_execution_time -= duration;
        // std::cout << "task id: " << id << std::endl;
        // std::cout << remaining_execution_time << std::endl;
        assert(sim()->round_zero(remaining_execution_time) >= 0);
}

void task::next_job()
{
        remaining_execution_time = pending_jobs.front();
        pending_jobs.pop();
}
