#include "task.hpp"

#include <limits>
#include <cassert>

task::task(const int id, const double& period, const double& utilization):
    id(id), period(period), utilization(utilization){}

void task::set_remaining_execution_time(const double& new_remaining_execution_time) {
	if (new_remaining_execution_time <= 0 || new_remaining_execution_time > std::numeric_limits<double>::max()) {
		assert("Trying to put define a out-of-bound remaining execution time");
		return;
	}

	this->remaining_execution_time = new_remaining_execution_time;
}

auto task::is_attached() -> bool {
	return (attached_proc.get() != nullptr);
}

auto task::has_remaining_time() -> bool {
	return (this->remaining_execution_time > 0);
}
