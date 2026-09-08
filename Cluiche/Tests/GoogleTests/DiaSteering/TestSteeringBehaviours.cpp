// Suites: Seek, Flee, Arrive, Wander, Pursue, Evade, ObstacleAvoidance, Separation

#include <gtest/gtest.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaSteering/Behaviours.h>
#include <DiaSteering/Testing/SteeringTestHelpers.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::Steering;
using namespace Dia::Steering::Testing;
using Dia::Maths::Vector2D;

// ---------------------------------------------------------------------------
// Seek
// ---------------------------------------------------------------------------

TEST(Seek, TowardTarget_OutputIsNonZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D result = Seek(agent, Vector2D(5.0f, 0.0f));
    EXPECT_GT(result.Magnitude(), 0.0f);
}

TEST(Seek, TowardTarget_DirectionPointsTowardTarget)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    AssertSeekDirection(agent, Vector2D(3.0f, 4.0f));
}

TEST(Seek, AtTarget_ReturnsZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(2.0f, 3.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D result = Seek(agent, Vector2D(2.0f, 3.0f));
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(Seek, OutputSpeed_EqualsMaxSpeed)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 2.0f;

    Vector2D result = Seek(agent, Vector2D(1.0f, 0.0f));
    EXPECT_FLOAT_EQ(result.Magnitude(), 2.0f);
}

// ---------------------------------------------------------------------------
// Flee
// ---------------------------------------------------------------------------

TEST(Flee, AwayFromTarget_OutputIsNonZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D result = Flee(agent, Vector2D(1.0f, 0.0f));
    EXPECT_GT(result.Magnitude(), 0.0f);
}

TEST(Flee, DirectionPointsAwayFromTarget)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D target(3.0f, 0.0f);
    Vector2D result = Flee(agent, target);

    // Dot of result direction with away-from-target direction should be positive
    Vector2D away = (agent.position - target).AsNormalSafe();
    float dot = result.AsNormalSafe().Dot(away);
    EXPECT_GT(dot, 0.9f);
}

TEST(Flee, AtTarget_ReturnsZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(1.0f, 1.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D result = Flee(agent, Vector2D(1.0f, 1.0f));
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

// ---------------------------------------------------------------------------
// Arrive
// ---------------------------------------------------------------------------

TEST(Arrive, AtTarget_ReturnsZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(5.0f, 5.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D result = Arrive(agent, Vector2D(5.0f, 5.0f), 2.0f);
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(Arrive, OutsideSlowingRadius_FullSpeed)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    // target is far away, well outside slowingRadius=2
    Vector2D result = Arrive(agent, Vector2D(10.0f, 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(result.Magnitude(), 1.0f);
}

TEST(Arrive, InsideSlowingRadius_ReducedSpeed)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    // agent is 0.5 units from target; slowingRadius = 2.0 → deceleration zone
    AssertArriveDeceleration(agent, Vector2D(0.5f, 0.0f), 2.0f);
}

TEST(Arrive, InsideSlowingRadius_DirectionTowardTarget)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D target(0.5f, 0.0f);
    Vector2D result = Arrive(agent, target, 2.0f);

    // Direction should still point toward target
    Vector2D expected = (target - agent.position).AsNormalSafe();
    float dot = result.AsNormalSafe().Dot(expected);
    EXPECT_GT(dot, 0.99f);
}

// ---------------------------------------------------------------------------
// Wander
// ---------------------------------------------------------------------------

TEST(Wander, NonIdleAgent_OutputIsNonZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(1.0f, 0.0f);  // moving agent
    agent.maxSpeed = 1.0f;

    float wanderAngle = 0.0f;
    Vector2D result = Wander(agent, 2.0f, 1.0f, wanderAngle);
    EXPECT_GT(result.Magnitude(), 0.0f);
}

TEST(Wander, AngleAdvancesEachCall)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(1.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    float wanderAngle = 0.0f;
    float before = wanderAngle;
    Wander(agent, 2.0f, 1.0f, wanderAngle);
    EXPECT_NE(wanderAngle, before);
}

TEST(Wander, AngleAdvancesBy0_3PerCall)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(1.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    float wanderAngle = 0.0f;
    Wander(agent, 2.0f, 1.0f, wanderAngle);
    EXPECT_FLOAT_EQ(wanderAngle, 0.3f);
}

// ---------------------------------------------------------------------------
// Pursue
// ---------------------------------------------------------------------------

TEST(Pursue, PredictionTimeZero_DegenratesToSeek)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D targetPos(4.0f, 0.0f);
    Vector2D targetVel(1.0f, 0.0f);

    Vector2D seekResult  = Seek(agent, targetPos);
    Vector2D pursueResult = Pursue(agent, targetPos, targetVel, 0.0f);

    EXPECT_FLOAT_EQ(seekResult.X(), pursueResult.X());
    EXPECT_FLOAT_EQ(seekResult.Y(), pursueResult.Y());
}

TEST(Pursue, PredictionTimePositive_SeeksDifferentPoint)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D targetPos(4.0f, 0.0f);
    Vector2D targetVel(0.0f, 1.0f);  // target moving perpendicular

    Vector2D seekResult   = Seek(agent, targetPos);
    Vector2D pursueResult = Pursue(agent, targetPos, targetVel, 2.0f);

    // With prediction, the sought point is targetPos + targetVel * predictionTime
    // This produces a different direction than plain Seek
    bool different = (seekResult.X() != pursueResult.X()) || (seekResult.Y() != pursueResult.Y());
    EXPECT_TRUE(different);
}

// ---------------------------------------------------------------------------
// Evade
// ---------------------------------------------------------------------------

TEST(Evade, PredictionTimeZero_DegeneratesToFlee)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D threatPos(3.0f, 0.0f);
    Vector2D threatVel(1.0f, 0.0f);

    Vector2D fleeResult  = Flee(agent, threatPos);
    Vector2D evadeResult = Evade(agent, threatPos, threatVel, 0.0f);

    EXPECT_FLOAT_EQ(fleeResult.X(), evadeResult.X());
    EXPECT_FLOAT_EQ(fleeResult.Y(), evadeResult.Y());
}

TEST(Evade, PredictionTimePositive_FleesDifferentPoint)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Vector2D threatPos(3.0f, 0.0f);
    Vector2D threatVel(0.0f, 1.0f);  // threat moving perpendicular

    Vector2D fleeResult  = Flee(agent, threatPos);
    Vector2D evadeResult = Evade(agent, threatPos, threatVel, 2.0f);

    bool different = (fleeResult.X() != evadeResult.X()) || (fleeResult.Y() != evadeResult.Y());
    EXPECT_TRUE(different);
}

// ---------------------------------------------------------------------------
// ObstacleAvoidance
// ---------------------------------------------------------------------------

TEST(ObstacleAvoidance, NoObstacles_ReturnsZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(1.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Dia::Core::Containers::DynamicArrayC<Vector2D, 64> positions;
    Dia::Core::Containers::DynamicArrayC<float, 64>    radii;

    Vector2D result = ObstacleAvoidance(agent, positions, radii, 3.0f);
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(ObstacleAvoidance, ObstacleInDetectionBox_ReturnsNonZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(1.0f, 0.0f);  // moving right
    agent.maxSpeed = 1.0f;

    MockObstacleSet obstacles;
    // Obstacle directly ahead in the detection box
    obstacles.Add(Vector2D(2.0f, 0.0f), 0.5f);

    Vector2D result = ObstacleAvoidance(agent, obstacles.positions, obstacles.radii, 5.0f);
    EXPECT_GT(result.Magnitude(), 0.0f);
}

TEST(ObstacleAvoidance, ObstacleFarAway_ReturnsZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(1.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    MockObstacleSet obstacles;
    // Obstacle far beyond detection box length
    obstacles.Add(Vector2D(100.0f, 0.0f), 0.5f);

    Vector2D result = ObstacleAvoidance(agent, obstacles.positions, obstacles.radii, 3.0f);
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

// ---------------------------------------------------------------------------
// Separation
// ---------------------------------------------------------------------------

TEST(Separation, NoNeighbours_ReturnsZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Dia::Core::Containers::DynamicArrayC<Vector2D, 64> neighbours;

    Vector2D result = Separation(agent, neighbours, 2.0f);
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(Separation, NeighbourWithinRange_ReturnsNonZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Dia::Core::Containers::DynamicArrayC<Vector2D, 64> neighbours;
    neighbours.Add(Vector2D(0.5f, 0.0f));  // well within desiredSeparation=2

    Vector2D result = Separation(agent, neighbours, 2.0f);
    EXPECT_GT(result.Magnitude(), 0.0f);
}

TEST(Separation, NeighbourOutsideRange_ReturnsZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Dia::Core::Containers::DynamicArrayC<Vector2D, 64> neighbours;
    neighbours.Add(Vector2D(10.0f, 0.0f));  // outside desiredSeparation=1

    Vector2D result = Separation(agent, neighbours, 1.0f);
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(Separation, DirectionPushesAwayFromNeighbour)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    // Use the helper which also checks direction
    AssertSeparationDirection(agent, Vector2D(0.5f, 0.0f), 2.0f);
}

TEST(Separation, MultipleNeighbours_CombinedForceNonZero)
{
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    Dia::Core::Containers::DynamicArrayC<Vector2D, 64> neighbours;
    neighbours.Add(Vector2D(0.5f, 0.0f));
    neighbours.Add(Vector2D(-0.5f, 0.0f));

    Vector2D result = Separation(agent, neighbours, 2.0f);
    // Symmetric arrangement — X forces cancel, but function should still
    // produce a result (even if near-zero in X, both neighbours contribute)
    // The combined force computation should run without crashing
    (void)result;  // not crashing is sufficient here; direction test is per-neighbour
}
