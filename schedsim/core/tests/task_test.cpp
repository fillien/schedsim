#include <schedsim/core/task.hpp>
#include <schedsim/core/processor_type.hpp>

#include <gtest/gtest.h>

using namespace schedsim::core;

TEST(TaskTest, Construction) {
    Task task(0, Duration{10.0}, Duration{8.0}, Duration{2.0});

    EXPECT_EQ(task.id(), 0U);
    EXPECT_EQ(task.period().count(), 10.0);
    EXPECT_EQ(task.relative_deadline().count(), 8.0);
    EXPECT_EQ(task.wcet().count(), 2.0);
}

TEST(TaskTest, ImplicitDeadlineTask) {
    // Deadline == period
    Task task(0, Duration{10.0}, Duration{10.0}, Duration{3.0});

    EXPECT_EQ(task.period().count(), task.relative_deadline().count());
}

TEST(TaskTest, ConstrainedDeadlineTask) {
    // Deadline < period
    Task task(0, Duration{10.0}, Duration{5.0}, Duration{2.0});

    EXPECT_LT(task.relative_deadline().count(), task.period().count());
}

TEST(TaskTest, PerTypeWcetSamePerformance) {
    Task task(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    ProcessorType pt(0, "reference", 1.0);

    // With reference performance = 1.0 and type performance = 1.0
    // wcet should be unchanged
    Duration per_type_wcet = task.wcet(pt, 1.0);
    EXPECT_DOUBLE_EQ(per_type_wcet.count(), 2.0);
}

TEST(TaskTest, PerTypeWcetSlowerProcessor) {
    Task task(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    ProcessorType pt(0, "slow", 0.5);

    // With reference performance = 1.0 and type performance = 0.5
    // wcet should be 2.0 / (0.5/1.0) = 4.0
    Duration per_type_wcet = task.wcet(pt, 1.0);
    EXPECT_DOUBLE_EQ(per_type_wcet.count(), 4.0);
}

TEST(TaskTest, PerTypeWcetFasterProcessor) {
    Task task(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    ProcessorType pt(0, "fast", 2.0);

    // With reference performance = 1.0 and type performance = 2.0
    // wcet should be 2.0 / (2.0/1.0) = 1.0
    Duration per_type_wcet = task.wcet(pt, 1.0);
    EXPECT_DOUBLE_EQ(per_type_wcet.count(), 1.0);
}

TEST(TaskTest, PerTypeWcetWithHigherReference) {
    Task task(0, Duration{10.0}, Duration{10.0}, Duration{2.0});
    ProcessorType pt(0, "medium", 1.0);

    // With reference performance = 2.0 and type performance = 1.0
    // wcet should be 2.0 / (1.0/2.0) = 4.0
    Duration per_type_wcet = task.wcet(pt, 2.0);
    EXPECT_DOUBLE_EQ(per_type_wcet.count(), 4.0);
}

TEST(TaskTest, MoveConstruction) {
    Task task1(0, Duration{10.0}, Duration{8.0}, Duration{2.0});
    Task task2(std::move(task1));

    EXPECT_EQ(task2.id(), 0U);
    EXPECT_EQ(task2.period().count(), 10.0);
    EXPECT_EQ(task2.relative_deadline().count(), 8.0);
    EXPECT_EQ(task2.wcet().count(), 2.0);
}

TEST(TaskTest, MoveAssignment) {
    Task task1(0, Duration{10.0}, Duration{8.0}, Duration{2.0});
    Task task2(1, Duration{20.0}, Duration{20.0}, Duration{5.0});

    task2 = std::move(task1);

    EXPECT_EQ(task2.id(), 0U);
    EXPECT_EQ(task2.period().count(), 10.0);
}
