#include "dawn/core/pipeline/task_pipeline.h"

#include <gtest/gtest.h>

#include <utility>

using namespace dawn::core;

TEST(TaskPipelineState, ProgressesThroughSuccess) {
    TaskPlan plan;
    plan.id = "task-1";
    plan.title = "Download mod";
    plan.steps = {
        {"resolve", "Resolve", TaskStatus::Pending, 0, {}},
        {"download", "Download", TaskStatus::Pending, 0, {}},
        {"install", "Install", TaskStatus::Pending, 0, {}},
    };

    TaskPipeline pipeline(std::move(plan));
    EXPECT_EQ(pipeline.plan().status, TaskStatus::Pending);

    pipeline.start();
    EXPECT_EQ(pipeline.plan().status, TaskStatus::Running);

    pipeline.advance_step("resolve", TaskStatus::Succeeded, "done");
    EXPECT_EQ(pipeline.plan().status, TaskStatus::Running);

    pipeline.pause();
    EXPECT_EQ(pipeline.plan().status, TaskStatus::Paused);

    pipeline.resume();
    EXPECT_EQ(pipeline.plan().status, TaskStatus::Running);

    pipeline.advance_step("download", TaskStatus::Succeeded, "done");
    pipeline.advance_step("install", TaskStatus::Succeeded, "done");
    EXPECT_EQ(pipeline.plan().status, TaskStatus::Succeeded);

    const auto result = pipeline.finish("completed");
    EXPECT_EQ(result.status, TaskStatus::Succeeded);
    EXPECT_EQ(result.planId, "task-1");
    EXPECT_EQ(result.logs.size(), 3u);
}

TEST(TaskPipelineState, MarksFailure) {
    TaskPlan plan;
    plan.id = "task-2";
    plan.title = "Repair";
    plan.steps = {
        {"resolve", "Resolve", TaskStatus::Pending, 0, {}},
        {"install", "Install", TaskStatus::Pending, 0, {}},
    };

    TaskPipeline pipeline(std::move(plan));
    pipeline.start();
    pipeline.advance_step("resolve", TaskStatus::Failed, "broken dependency");
    EXPECT_EQ(pipeline.plan().status, TaskStatus::Failed);
}
