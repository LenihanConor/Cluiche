// Suite: SteeringSystem

#include <gtest/gtest.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaSteering/SteeringPipeline.h>
#include <DiaSteering/SteeringSystem.h>
#include <DiaSteering/Behaviours.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

#include <utility>

using namespace Dia::Steering;
using Dia::Maths::Vector2D;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// AddAgent / GetAgentCount
// ---------------------------------------------------------------------------

TEST(SteeringSystem, AddAgent_IncrementsCount)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    system.AddAgent(StringCRC("agent_a"), agent);
    EXPECT_EQ(system.GetAgentCount(), 1);
}

TEST(SteeringSystem, AddTwoAgents_CountIsTwo)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    system.AddAgent(StringCRC("agent_a"), agent);
    system.AddAgent(StringCRC("agent_b"), agent);
    EXPECT_EQ(system.GetAgentCount(), 2);
}

TEST(SteeringSystem, AddSameIdTwice_CountRemainsOne)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    system.AddAgent(StringCRC("agent_a"), agent);
    system.AddAgent(StringCRC("agent_a"), agent);  // overwrite
    EXPECT_EQ(system.GetAgentCount(), 1);
}

// ---------------------------------------------------------------------------
// RemoveAgent
// ---------------------------------------------------------------------------

TEST(SteeringSystem, RemoveAgent_DecrementsCount)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    StringCRC id("agent_a");
    system.AddAgent(id, agent);
    system.RemoveAgent(id);
    EXPECT_EQ(system.GetAgentCount(), 0);
}

TEST(SteeringSystem, RemoveUnknownAgent_CountUnchanged)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    system.AddAgent(StringCRC("agent_a"), agent);
    system.RemoveAgent(StringCRC("nobody"));
    EXPECT_EQ(system.GetAgentCount(), 1);
}

// ---------------------------------------------------------------------------
// GetOutput
// ---------------------------------------------------------------------------

TEST(SteeringSystem, GetOutput_UnknownAgent_ReturnsZero)
{
    SteeringSystem system;

    Vector2D result = system.GetOutput(StringCRC("ghost"));
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(SteeringSystem, GetOutput_BeforeUpdate_ReturnsZero)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    StringCRC id("agent_a");
    system.AddAgent(id, agent);

    // No Update() called yet
    Vector2D result = system.GetOutput(id);
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

// ---------------------------------------------------------------------------
// Update — output caching
// ---------------------------------------------------------------------------

TEST(SteeringSystem, Update_CachesOutputFromPipeline)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    StringCRC id("agent_a");
    system.AddAgent(id, agent);

    // Build a pipeline with a Seek toward (1,0)
    Dia::Core::Containers::DynamicArrayC<std::pair<SteeringAgentId, SteeringPipeline>, 256> pipelines;
    SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f, Seek(agent, Vector2D(1.0f, 0.0f)), 1.0f);
    pipelines.Add(std::make_pair(id, pipeline));

    system.Update(0.016f, pipelines);

    Vector2D result = system.GetOutput(id);
    EXPECT_GT(result.Magnitude(), 0.0f);
    EXPECT_FLOAT_EQ(result.X(), 1.0f);   // Seek toward (1,0) at maxSpeed=1
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

TEST(SteeringSystem, Update_PipelineZero_OutputIsZero)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    StringCRC id("agent_a");
    system.AddAgent(id, agent);

    // Pipeline with zero contribution
    Dia::Core::Containers::DynamicArrayC<std::pair<SteeringAgentId, SteeringPipeline>, 256> pipelines;
    SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f, Vector2D(0.0f, 0.0f), 1.0f);
    pipelines.Add(std::make_pair(id, pipeline));

    system.Update(0.016f, pipelines);

    Vector2D result = system.GetOutput(id);
    EXPECT_FLOAT_EQ(result.X(), 0.0f);
    EXPECT_FLOAT_EQ(result.Y(), 0.0f);
}

// ---------------------------------------------------------------------------
// UpdateAgentState
// ---------------------------------------------------------------------------

TEST(SteeringSystem, UpdateAgentState_ReplacesAgentData)
{
    SteeringSystem system;
    SteeringAgent agent;
    agent.position = Vector2D(0.0f, 0.0f);
    agent.velocity = Vector2D(0.0f, 0.0f);
    agent.maxSpeed = 1.0f;

    StringCRC id("agent_a");
    system.AddAgent(id, agent);

    // Move the agent to a new position
    SteeringAgent updated;
    updated.position = Vector2D(10.0f, 5.0f);
    updated.velocity = Vector2D(1.0f, 0.0f);
    updated.maxSpeed = 2.0f;
    system.UpdateAgentState(id, updated);

    // Build pipeline using the updated agent's state (Seek from new position)
    Dia::Core::Containers::DynamicArrayC<std::pair<SteeringAgentId, SteeringPipeline>, 256> pipelines;
    SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f, Seek(updated, Vector2D(10.0f, 6.0f)), 1.0f);
    pipelines.Add(std::make_pair(id, pipeline));

    system.Update(0.016f, pipelines);

    // Output should be non-zero — agent moved so Seek is valid
    Vector2D result = system.GetOutput(id);
    EXPECT_GT(result.Magnitude(), 0.0f);
}

TEST(SteeringSystem, UpdateAgentState_UnknownId_IsNoOp)
{
    SteeringSystem system;
    // No agents registered; should not crash
    SteeringAgent updated;
    updated.position = Vector2D(1.0f, 1.0f);
    updated.velocity = Vector2D(0.0f, 0.0f);
    updated.maxSpeed = 1.0f;

    system.UpdateAgentState(StringCRC("nobody"), updated);
    EXPECT_EQ(system.GetAgentCount(), 0);
}

// ---------------------------------------------------------------------------
// Multiple agents — independent outputs
// ---------------------------------------------------------------------------

TEST(SteeringSystem, TwoAgents_IndependentOutputs)
{
    SteeringSystem system;

    SteeringAgent agentA;
    agentA.position = Vector2D(0.0f, 0.0f);
    agentA.velocity = Vector2D(0.0f, 0.0f);
    agentA.maxSpeed = 1.0f;

    SteeringAgent agentB;
    agentB.position = Vector2D(5.0f, 5.0f);
    agentB.velocity = Vector2D(0.0f, 0.0f);
    agentB.maxSpeed = 1.0f;

    StringCRC idA("agent_a");
    StringCRC idB("agent_b");

    system.AddAgent(idA, agentA);
    system.AddAgent(idB, agentB);

    Dia::Core::Containers::DynamicArrayC<std::pair<SteeringAgentId, SteeringPipeline>, 256> pipelines;

    SteeringPipeline pipelineA;
    pipelineA.AddContribution(0, 1.0f, Seek(agentA, Vector2D(1.0f, 0.0f)), 1.0f);
    pipelines.Add(std::make_pair(idA, pipelineA));

    SteeringPipeline pipelineB;
    pipelineB.AddContribution(0, 1.0f, Seek(agentB, Vector2D(5.0f, 6.0f)), 1.0f);
    pipelines.Add(std::make_pair(idB, pipelineB));

    system.Update(0.016f, pipelines);

    Vector2D outA = system.GetOutput(idA);
    Vector2D outB = system.GetOutput(idB);

    // Both should be non-zero
    EXPECT_GT(outA.Magnitude(), 0.0f);
    EXPECT_GT(outB.Magnitude(), 0.0f);

    // They should be in different directions
    bool different = (outA.X() != outB.X()) || (outA.Y() != outB.Y());
    EXPECT_TRUE(different);
}
