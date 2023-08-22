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
