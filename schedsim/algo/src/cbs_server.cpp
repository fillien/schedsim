#include <schedsim/algo/cbs_server.hpp>

#include <cassert>
#include <stdexcept>

namespace schedsim::algo {

CbsServer::CbsServer(std::size_t id, core::Duration budget, core::Duration period,
                     OverrunPolicy policy)
    : id_(id)
    , budget_(budget)
    , period_(period)
    , utilization_(budget.count() / period.count())
    , overrun_policy_(policy)
    , remaining_budget_(budget) {
    assert(budget.count() > 0.0 && "Budget must be positive");
    assert(period.count() > 0.0 && "Period must be positive");
    assert(budget.count() <= period.count() && "Budget cannot exceed period");
}

core::Job* CbsServer::current_job() {
    return job_queue_.empty() ? nullptr : &job_queue_.front();
}

const core::Job* CbsServer::current_job() const {
    return job_queue_.empty() ? nullptr : &job_queue_.front();
}

void CbsServer::enqueue_job(core::Job job) {
    // Handle overrun policy if there's already a running job
    if (state_ == State::Running && !job_queue_.empty()) {
        switch (overrun_policy_) {
            case OverrunPolicy::Queue:
                // Default: just enqueue
                break;
            case OverrunPolicy::Skip:
                // Drop the new job
                return;
            case OverrunPolicy::Abort:
                // Abort current job (remove from front)
                job_queue_.pop_front();
                break;
        }
    }
    job_queue_.push_back(std::move(job));
}

core::Job CbsServer::dequeue_job() {
    assert(!job_queue_.empty() && "Cannot dequeue from empty queue");
    core::Job job = std::move(job_queue_.front());
    job_queue_.pop_front();
    return job;
}

void CbsServer::activate(core::TimePoint current_time) {
    assert(state_ == State::Inactive && "Can only activate from Inactive state");
    assert(!job_queue_.empty() && "Must have pending jobs to activate");

    // Initialize CBS state
    virtual_time_ = current_time;
    deadline_ = current_time + period_;
    remaining_budget_ = budget_;
    state_ = State::Ready;
}

void CbsServer::dispatch() {
    assert(state_ == State::Ready && "Can only dispatch from Ready state");
    assert(!job_queue_.empty() && "Must have pending jobs to dispatch");

    state_ = State::Running;
}

void CbsServer::preempt() {
    assert(state_ == State::Running && "Can only preempt from Running state");

    state_ = State::Ready;
}

void CbsServer::complete_job(core::TimePoint /*current_time*/) {
    assert(state_ == State::Running && "Can only complete job from Running state");

    if (has_pending_jobs() && job_queue_.size() > 1) {
        // More jobs in queue (besides the one completing), stay Ready
        state_ = State::Ready;
    } else {
        // No more jobs, go Inactive
        state_ = State::Inactive;
    }
}

void CbsServer::exhaust_budget(core::TimePoint /*current_time*/) {
    assert(state_ == State::Running && "Can only exhaust budget from Running state");

    // Postpone deadline and replenish budget
    postpone_deadline();

    // Transition to Ready (will be re-dispatched with new deadline)
    state_ = State::Ready;
}

void CbsServer::update_virtual_time(core::Duration execution_time) {
    // CBS formula: vt += execution_time / U
    // This represents the progress in "virtual" time based on utilization
    core::Duration vt_increment{execution_time.count() / utilization_};
    virtual_time_ = core::TimePoint{virtual_time_.time_since_epoch() + vt_increment};
}

void CbsServer::postpone_deadline() {
    // CBS formula: d += T, budget = Q
    deadline_ += period_;
    remaining_budget_ = budget_;
}

void CbsServer::consume_budget(core::Duration amount) {
    assert(amount.count() >= 0.0 && "Cannot consume negative budget");
    remaining_budget_ -= amount;
    if (remaining_budget_.count() < 0.0) {
        remaining_budget_ = core::Duration{0.0};
    }
}

} // namespace schedsim::algo
