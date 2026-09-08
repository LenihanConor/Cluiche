// Suite: SteeringPipeline

#include <gtest/gtest.h>
#include <DiaSteering/SteeringPipeline.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Steering;
using Dia::Maths::Vector2D;

// ---------------------------------------------------------------------------
// Empty pipeline
// ---------------------------------------------------------------------------

TEST(SteeringPipeline, Empty_ReturnsZero)
{
    SteeringPipeline pipeline;
    Vector2D result = pipeline.Evaluate();
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

// ---------------------------------------------------------------------------
// Single contribution
// ---------------------------------------------------------------------------

TEST(SteeringPipeline, SingleContribution_ReturnedDirectly)
{
    SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f, Vector2D(1.0f, 0.0f), 1.0f);

    Vector2D result = pipeline.Evaluate();
    EXPECT_FLOAT_EQ(result.X(), 1.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

// ---------------------------------------------------------------------------
// Priority ordering (SD-003)
// ---------------------------------------------------------------------------

TEST(SteeringPipeline, HighPriorityGroupNonZero_WinsOverLow)
{
    SteeringPipeline pipeline;
    // priority 0 (higher) — non-zero
    pipeline.AddContribution(0, 1.0f, Vector2D(1.0f, 0.0f), 1.0f);
    // priority 1 (lower) — different direction
    pipeline.AddContribution(1, 1.0f, Vector2D(0.0f, 1.0f), 1.0f);

    Vector2D result = pipeline.Evaluate();
    // priority-0 group should win
    EXPECT_FLOAT_EQ(result.X(), 1.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(SteeringPipeline, SD003_HighPriorityZero_FallsBackToLower)
{
    SteeringPipeline pipeline;
    // priority 0 (higher) — zero output
    pipeline.AddContribution(0, 1.0f, Vector2D(0.0f, 0.0f), 1.0f);
    // priority 1 (lower) — non-zero
    pipeline.AddContribution(1, 1.0f, Vector2D(0.0f, 1.0f), 1.0f);

    Vector2D result = pipeline.Evaluate();
    // priority-0 is zero → fall back to priority-1
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 1.0f);
}

// ---------------------------------------------------------------------------
// Within-group weighted average
// ---------------------------------------------------------------------------

TEST(SteeringPipeline, SameGroup_WeightedAverageIsCorrect)
{
    SteeringPipeline pipeline;
    // Two contributions in the same group, equal weight of 1.0
    // (1,0)*1 + (0,1)*1 → weighted average = (0.5, 0.5)
    pipeline.AddContribution(0, 1.0f, Vector2D(1.0f, 0.0f), 1.0f);
    pipeline.AddContribution(0, 1.0f, Vector2D(0.0f, 1.0f), 1.0f);

    Vector2D result = pipeline.Evaluate();
    EXPECT_FLOAT_EQ(result.X(), 0.5f);
    EXPECT_FLOAT_EQ(result.Y(), 0.5f);
}

TEST(SteeringPipeline, SameGroup_UnequalWeights_BlendedCorrectly)
{
    SteeringPipeline pipeline;
    // (2,0) weight=2 and (0,2) weight=1 → (2*2 + 0*1)/(2+1)=4/3, (0*2+2*1)/(2+1)=2/3
    pipeline.AddContribution(0, 1.0f, Vector2D(2.0f, 0.0f), 2.0f);
    pipeline.AddContribution(0, 1.0f, Vector2D(0.0f, 2.0f), 1.0f);

    Vector2D result = pipeline.Evaluate();
    EXPECT_FLOAT_EQ(result.X(), 4.0f / 3.0f);
    EXPECT_FLOAT_EQ(result.Y(), 2.0f / 3.0f);
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST(SteeringPipeline, Clear_ResetsToZero)
{
    SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f, Vector2D(1.0f, 0.0f), 1.0f);

    pipeline.Clear();

    Vector2D result = pipeline.Evaluate();
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(SteeringPipeline, ClearThenRefill_CorrectResult)
{
    SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f, Vector2D(1.0f, 0.0f), 1.0f);
    pipeline.Clear();

    pipeline.AddContribution(0, 1.0f, Vector2D(0.0f, 1.0f), 1.0f);
    Vector2D result = pipeline.Evaluate();
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 1.0f);
}

// ---------------------------------------------------------------------------
// All groups zero
// ---------------------------------------------------------------------------

TEST(SteeringPipeline, AllGroupsZero_ReturnsZero)
{
    SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f, Vector2D(0.0f, 0.0f), 1.0f);
    pipeline.AddContribution(1, 1.0f, Vector2D(0.0f, 0.0f), 1.0f);

    Vector2D result = pipeline.Evaluate();
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}
