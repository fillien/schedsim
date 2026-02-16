#include <schedsim/core/error.hpp>
#include <schedsim/core/job.hpp>
#include <schedsim/core/task.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

class JobTest : public ::testing::Test {
protected:
    Task task_{0, Duration{10.0}, Duration{10.0}, Duration{3.0}};
};

TEST_F(JobTest, Construction) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    EXPECT_EQ(&job.task(), &task_);
    EXPECT_EQ(job.total_work().count(), 3.0);
    EXPECT_EQ(job.remaining_work().count(), 3.0);
    EXPECT_EQ(job.absolute_deadline(), deadline);
    EXPECT_FALSE(job.is_complete());
}

TEST_F(JobTest, ConsumeWork) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    job.consume_work(Duration{1.0});
    EXPECT_EQ(job.remaining_work().count(), 2.0);
    EXPECT_FALSE(job.is_complete());

    job.consume_work(Duration{1.5});
    EXPECT_EQ(job.remaining_work().count(), 0.5);
    EXPECT_FALSE(job.is_complete());
}

TEST_F(JobTest, JobCompletion) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    job.consume_work(Duration{3.0});
    EXPECT_EQ(job.remaining_work().count(), 0.0);
    EXPECT_TRUE(job.is_complete());
}

TEST_F(JobTest, OverConsumeWorkClampsToZero) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    // Over-consumption clamped to zero (floating-point error from DVFS)
    job.consume_work(Duration{5.0});
    EXPECT_EQ(job.remaining_work().count(), 0.0);
    EXPECT_TRUE(job.is_complete());
}

TEST_F(JobTest, TinyNegativeRoundingClamped) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    // Over-consumption within tolerance (1e-6) is clamped to zero
    job.consume_work(Duration{3.0 + 1e-9});
    EXPECT_EQ(job.remaining_work().count(), 0.0);
    EXPECT_TRUE(job.is_complete());
}

TEST_F(JobTest, TotalWorkUnchanged) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    job.consume_work(Duration{1.0});
    job.consume_work(Duration{1.0});

    EXPECT_EQ(job.total_work().count(), 3.0);
    EXPECT_EQ(job.remaining_work().count(), 1.0);
}

TEST_F(JobTest, MoveConstruction) {
    TimePoint deadline{Duration{10.0}};
    Job job1(task_, Duration{3.0}, deadline);
    job1.consume_work(Duration{1.0});

    Job job2(std::move(job1));

    EXPECT_EQ(&job2.task(), &task_);
    EXPECT_EQ(job2.total_work().count(), 3.0);
    EXPECT_EQ(job2.remaining_work().count(), 2.0);
}

TEST_F(JobTest, MoveAssignment) {
    TimePoint deadline1{Duration{10.0}};
    TimePoint deadline2{Duration{20.0}};

    Job job1(task_, Duration{3.0}, deadline1);
    job1.consume_work(Duration{1.0});

    Job job2(task_, Duration{5.0}, deadline2);

    job2 = std::move(job1);

    EXPECT_EQ(job2.total_work().count(), 3.0);
    EXPECT_EQ(job2.remaining_work().count(), 2.0);
    EXPECT_EQ(job2.absolute_deadline(), deadline1);
}

TEST_F(JobTest, ConsumeWork_OverConsumptionClamped) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    // Any over-consumption is clamped to zero (floating-point error from DVFS)
    job.consume_work(Duration{3.0 + 0.01});
    EXPECT_EQ(job.remaining_work().count(), 0.0);
    EXPECT_TRUE(job.is_complete());
}

TEST_F(JobTest, ConsumeWork_AccumulatedDVFSRounding) {
    TimePoint deadline{Duration{10.0}};
    Job job(task_, Duration{3.0}, deadline);

    // Simulate DVFS fractional speeds: 3 steps of (1.0 + 1e-10)
    job.consume_work(Duration{1.0 + 1e-10});
    job.consume_work(Duration{1.0 + 1e-10});
    job.consume_work(Duration{1.0 + 1e-10});

    EXPECT_EQ(job.remaining_work().count(), 0.0);
    EXPECT_TRUE(job.is_complete());
}
