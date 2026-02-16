#include <schedsim/core/processor.hpp>
#include <schedsim/core/clock_domain.hpp>
#include <schedsim/core/engine.hpp>
#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/platform.hpp>
#include <schedsim/core/power_domain.hpp>
#include <schedsim/core/processor_type.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class ProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        type_ = std::make_unique<ProcessorType>(0, "big", 1.0);
        clock_domain_ = std::make_unique<ClockDomain>(0, Frequency{500.0}, Frequency{2000.0});
        power_domain_ = std::make_unique<PowerDomain>(0, std::vector<CStateLevel>{
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });
    }

    std::unique_ptr<ProcessorType> type_;
    std::unique_ptr<ClockDomain> clock_domain_;
    std::unique_ptr<PowerDomain> power_domain_;
};

TEST_F(ProcessorTest, Construction) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);

    EXPECT_EQ(proc.id(), 0U);
    EXPECT_EQ(&proc.type(), type_.get());
    EXPECT_EQ(&proc.clock_domain(), clock_domain_.get());
    EXPECT_EQ(&proc.power_domain(), power_domain_.get());
    EXPECT_EQ(proc.state(), ProcessorState::Idle);
    EXPECT_EQ(proc.current_job(), nullptr);
    EXPECT_EQ(proc.current_task(), nullptr);
}

TEST_F(ProcessorTest, SpeedCalculationMaxFreq) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);

    // At max frequency (2000), with performance = 1.0, reference = 1.0
    // Speed = (2000/2000) * (1.0/1.0) = 1.0
    EXPECT_DOUBLE_EQ(proc.speed(1.0), 1.0);
}

TEST_F(ProcessorTest, SpeedCalculationHalfFreq) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);
    clock_domain_->set_frequency(Frequency{1000.0});

    // At half frequency (1000), with performance = 1.0, reference = 1.0
    // Speed = (1000/2000) * (1.0/1.0) = 0.5
    EXPECT_DOUBLE_EQ(proc.speed(1.0), 0.5);
}

TEST_F(ProcessorTest, SpeedCalculationWithDifferentPerformance) {
    ProcessorType slow_type(1, "LITTLE", 0.5);
    Processor proc(0, slow_type, *clock_domain_, *power_domain_);

    // At max frequency (2000), with performance = 0.5, reference = 1.0
    // Speed = (2000/2000) * (0.5/1.0) = 0.5
    EXPECT_DOUBLE_EQ(proc.speed(1.0), 0.5);
}

TEST_F(ProcessorTest, SpeedCalculationWithHigherReference) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);

    // At max frequency, with performance = 1.0, reference = 2.0
    // Speed = (2000/2000) * (1.0/2.0) = 0.5
    EXPECT_DOUBLE_EQ(proc.speed(2.0), 0.5);
}

TEST_F(ProcessorTest, ClearOnIdleThrows) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);

    EXPECT_THROW(proc.clear(), InvalidStateError);
}

TEST_F(ProcessorTest, RequestCStateFromIdle) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);

    proc.request_cstate(1);
    EXPECT_EQ(proc.state(), ProcessorState::Sleep);
}

TEST_F(ProcessorTest, RequestCStateWhileRunningThrows) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);
    Task task(0, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    TimePoint deadline = time_from_seconds(10.0);
    Job job(task, duration_from_seconds(2.0), deadline);

    proc.assign(job);
    EXPECT_EQ(proc.state(), ProcessorState::Running);

    EXPECT_THROW(proc.request_cstate(1), InvalidStateError);
}

TEST_F(ProcessorTest, HandlerRegistration) {
    Processor proc(0, *type_, *clock_domain_, *power_domain_);

    bool completion_called = false;
    bool deadline_called = false;
    bool available_called = false;

    proc.set_job_completion_handler([&](Processor&, Job&) {
        completion_called = true;
    });
    proc.set_deadline_miss_handler([&](Processor&, Job&) {
        deadline_called = true;
    });
    proc.set_processor_available_handler([&](Processor&) {
        available_called = true;
    });

    // Just verify handlers were set (they'll be called during simulation)
    EXPECT_FALSE(completion_called);
    EXPECT_FALSE(deadline_called);
    EXPECT_FALSE(available_called);
}

// Integration test with Engine
class ProcessorEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use platform to create components (proper wiring)
        auto& pt = engine_.platform().add_processor_type("big", 1.0);
        auto& cd = engine_.platform().add_clock_domain(Frequency{500.0}, Frequency{2000.0});
        auto& pd = engine_.platform().add_power_domain({
            {0, CStateScope::PerProcessor, duration_from_seconds(0.0), Power{100.0}}
        });
        proc_ = &engine_.platform().add_processor(pt, cd, pd);
        engine_.platform().finalize();

        task_ = std::make_unique<Task>(0, duration_from_seconds(10.0), duration_from_seconds(10.0), duration_from_seconds(2.0));
    }

    Engine engine_;
    Processor* proc_{nullptr};
    std::unique_ptr<Task> task_;
};

TEST_F(ProcessorEngineTest, AssignJob) {
    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);

    EXPECT_EQ(proc_->state(), ProcessorState::Running);
    EXPECT_EQ(proc_->current_job(), &job);
    EXPECT_EQ(proc_->current_task(), task_.get());
}

TEST_F(ProcessorEngineTest, AssignWhenNotIdleThrows) {
    TimePoint deadline = time_from_seconds(10.0);
    Job job1(*task_, duration_from_seconds(2.0), deadline);
    Job job2(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job1);

    EXPECT_THROW(proc_->assign(job2), InvalidStateError);
}

TEST_F(ProcessorEngineTest, ClearAfterAssign) {
    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);
    proc_->clear();

    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
    EXPECT_EQ(proc_->current_job(), nullptr);
}

TEST_F(ProcessorEngineTest, JobCompletionHandler) {
    bool handler_called = false;
    Job* completed_job = nullptr;

    proc_->set_job_completion_handler([&](Processor& p, Job& j) {
        handler_called = true;
        completed_job = &j;
        EXPECT_EQ(&p, proc_);
    });

    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);

    // Run simulation until completion (2.0 time units at speed 1.0)
    engine_.run(time_from_seconds(3.0));

    EXPECT_TRUE(handler_called);
    EXPECT_EQ(completed_job, &job);
    EXPECT_TRUE(job.is_complete());
}

TEST_F(ProcessorEngineTest, DeadlineMissHandler) {
    bool handler_called = false;

    proc_->set_deadline_miss_handler([&](Processor& p, Job& j) {
        handler_called = true;
        EXPECT_EQ(&p, proc_);
        (void)j;
    });

    // Job that takes longer than deadline
    TimePoint deadline = time_from_seconds(1.0);  // Deadline at t=1
    Job job(*task_, duration_from_seconds(2.0), deadline);  // Takes 2.0 time units

    proc_->assign(job);

    // Run simulation past deadline
    engine_.run(time_from_seconds(1.5));

    EXPECT_TRUE(handler_called);
}

TEST_F(ProcessorEngineTest, SpeedAffectsCompletionTime) {
    // Use a slower frequency
    proc_->clock_domain().set_frequency(Frequency{1000.0});  // Half speed

    bool handler_called = false;
    TimePoint completion_time;

    proc_->set_job_completion_handler([&](Processor&, Job&) {
        handler_called = true;
        completion_time = engine_.time();
    });

    TimePoint deadline = time_from_seconds(10.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);  // 2.0 reference units

    proc_->assign(job);

    // At speed 0.5, job should take 4.0 wall-clock seconds
    engine_.run(time_from_seconds(5.0));

    EXPECT_TRUE(handler_called);
    EXPECT_DOUBLE_EQ(time_to_seconds(completion_time), 4.0);
}

TEST_F(ProcessorEngineTest, DeadlineThenCompletion_SafeCancellation) {
    bool deadline_called = false;
    bool completion_called = false;

    proc_->set_deadline_miss_handler([&](Processor&, Job&) {
        deadline_called = true;
    });

    proc_->set_job_completion_handler([&](Processor&, Job&) {
        completion_called = true;
    });

    // Job: deadline at t=1.0, execution time 2.0
    // Deadline fires at t=1.0, completion fires at t=2.0
    TimePoint deadline = time_from_seconds(1.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);

    // Run past both events
    engine_.run(time_from_seconds(3.0));

    // Both handlers should have been called without crash
    EXPECT_TRUE(deadline_called);
    EXPECT_TRUE(completion_called);
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}

TEST_F(ProcessorEngineTest, CompletionAndDeadlineAtSameTime) {
    bool deadline_called = false;
    bool completion_called = false;

    proc_->set_deadline_miss_handler([&](Processor&, Job&) {
        deadline_called = true;
    });

    proc_->set_job_completion_handler([&](Processor&, Job&) {
        completion_called = true;
    });

    // Job: deadline at t=2.0, execution time 2.0
    // Both events at t=2.0 - completion should fire first (lower priority number)
    TimePoint deadline = time_from_seconds(2.0);
    Job job(*task_, duration_from_seconds(2.0), deadline);

    proc_->assign(job);

    engine_.run(time_from_seconds(3.0));

    // Completion fires first (priority 10), cancels deadline timer
    // Deadline handler should NOT be called
    EXPECT_TRUE(completion_called);
    EXPECT_FALSE(deadline_called);
    EXPECT_EQ(proc_->state(), ProcessorState::Idle);
}
