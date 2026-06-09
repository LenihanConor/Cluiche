#include <gtest/gtest.h>
#include <DiaApplicationFlowEditor/V2/RiskAssessor.h>
#include <DiaApplicationFlowEditor/V2/Commands/PUCommands.h>
#include <DiaApplicationFlowEditor/V2/Commands/ModuleCommands.h>
#include <DiaApplicationFlowEditor/V2/Commands/StreamCommands.h>
#include <DiaApplicationFlowEditor/V2/Commands/StageCommands.h>
#include <DiaApplicationFlowEditor/V2/ManifestEditorState.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::Core;

namespace
{
    ManifestEditorState MakeEmptyState()
    {
        ManifestEditorState state{};
        return state;
    }
} // anonymous namespace

// RemovePU

TEST(RiskAssessor, RemovePU_LiveConnected_HasRisk)
{
    RemovePUCommand cmd(StringCRC("TestPU"));
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, true);
    EXPECT_TRUE(report.HasRisk());
    EXPECT_EQ(report.condition, RiskCondition::RemovePU);
}

TEST(RiskAssessor, RemovePU_NotLive_NoRisk)
{
    RemovePUCommand cmd(StringCRC("TestPU"));
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, false);
    EXPECT_FALSE(report.HasRisk());
}

// RemoveModule

TEST(RiskAssessor, RemoveModule_LiveConnected_HasRisk)
{
    RemoveModuleCommand cmd(StringCRC("PU"), StringCRC("Mod"));
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, true);
    EXPECT_TRUE(report.HasRisk());
    EXPECT_EQ(report.condition, RiskCondition::RemoveModule);
}

TEST(RiskAssessor, RemoveModule_NotLive_NoRisk)
{
    RemoveModuleCommand cmd(StringCRC("PU"), StringCRC("Mod"));
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, false);
    EXPECT_FALSE(report.HasRisk());
}

// ChangeStreamCapacity

TEST(RiskAssessor, ChangeStreamCapacity_LiveConnected_HasRisk)
{
    SetStreamCapacityCommand cmd(StringCRC("Stream"), 32u);
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, true);
    EXPECT_TRUE(report.HasRisk());
    EXPECT_EQ(report.condition, RiskCondition::ChangeStreamCapacity);
}

TEST(RiskAssessor, ChangeStreamCapacity_NotLive_NoRisk)
{
    SetStreamCapacityCommand cmd(StringCRC("Stream"), 32u);
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, false);
    EXPECT_FALSE(report.HasRisk());
}

// ChangeStreamMaxReaders

TEST(RiskAssessor, ChangeStreamMaxReaders_LiveConnected_HasRisk)
{
    SetStreamMaxReadersCommand cmd(StringCRC("Stream"), 4u);
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, true);
    EXPECT_TRUE(report.HasRisk());
    EXPECT_EQ(report.condition, RiskCondition::ChangeStreamMaxReaders);
}

TEST(RiskAssessor, ChangeStreamMaxReaders_NotLive_NoRisk)
{
    SetStreamMaxReadersCommand cmd(StringCRC("Stream"), 4u);
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, false);
    EXPECT_FALSE(report.HasRisk());
}

// ChangeFrequency

TEST(RiskAssessor, ChangeFrequency_LiveConnected_HasRisk)
{
    SetPUFrequencyCommand cmd(StringCRC("TestPU"), 60.0f);
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, true);
    EXPECT_TRUE(report.HasRisk());
    EXPECT_EQ(report.condition, RiskCondition::ChangeFrequency);
}

TEST(RiskAssessor, ChangeFrequency_NotLive_NoRisk)
{
    SetPUFrequencyCommand cmd(StringCRC("TestPU"), 60.0f);
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, false);
    EXPECT_FALSE(report.HasRisk());
}

// RemoveStage

TEST(RiskAssessor, RemoveStage_LiveConnected_HasRisk)
{
    RemoveStageCommand cmd(StringCRC("Stage1"));
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, true);
    EXPECT_TRUE(report.HasRisk());
    EXPECT_EQ(report.condition, RiskCondition::RemoveStage);
}

TEST(RiskAssessor, RemoveStage_NotLive_NoRisk)
{
    RemoveStageCommand cmd(StringCRC("Stage1"));
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, false);
    EXPECT_FALSE(report.HasRisk());
}

// Safe command live — AddPU is not a risky operation

TEST(RiskAssessor, SafeCommand_LiveConnected_NoRisk)
{
    AddPUCommand cmd(StringCRC("NewPU"));
    ManifestEditorState state = MakeEmptyState();
    RiskReport report = RiskAssessor::Assess(cmd, state, true);
    EXPECT_FALSE(report.HasRisk());
}
