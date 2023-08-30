#include "task.hpp"

#include <limits>
#include <cassert>

task::task(const std::weak_ptr<engine> sim, const int id, const double& period, const double& utilization):
	entity(sim), id(id), period(period), utilization(utilization){}

auto task::is_attached() -> bool {
	return (attached_proc.get() != nullptr);
}

auto task::has_remaining_time() -> bool {
	return (this->remaining_execution_time > 0);
}

void task::add_job(const double& duration) {
        assert(duration >= 0);
        if (pending_jobs.empty() && remaining_execution_time == 0) {
		remaining_execution_time = duration;
        } else {
                pending_jobs.push(duration);
	}
}

void task::consume_time(const double& duration) {
        assert(duration >= 0);
	remaining_execution_time -= duration;
}

void task::next_job() {
        remaining_execution_time = pending_jobs.front();
	pending_jobs.pop();
}
