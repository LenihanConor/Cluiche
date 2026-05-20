#include <gtest/gtest.h>
#include <DiaApplicationEditor/V2/Commands/PUCommands.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

// ==============================================================================
// AddPUCommand
// ==============================================================================

TEST(Commands_PU, AddPU_Execute_PUPresent)
{
    ManifestEditorState state;
    AddPUCommand cmd(StringCRC("TestPU"));
    cmd.Execute(state);

    ASSERT_EQ(state.manifest.processingUnits.Size(), 1u);
    EXPECT_TRUE(state.manifest.processingUnits[0].instanceId == StringCRC("TestPU"));
}

TEST(Commands_PU, AddPU_Undo_PURemoved)
{
    ManifestEditorState state;
    AddPUCommand cmd(StringCRC("TestPU"));
    cmd.Execute(state);
    cmd.Undo(state);

    EXPECT_EQ(state.manifest.processingUnits.Size(), 0u);
}

// ==============================================================================
// RemovePUCommand
// ==============================================================================

TEST(Commands_PU, RemovePU_Execute_PUGone)
{
    ManifestEditorState state;
    AddPUCommand add(StringCRC("TestPU"));
    add.Execute(state);

    RemovePUCommand remove(StringCRC("TestPU"));
    remove.Execute(state);

    EXPECT_EQ(state.manifest.processingUnits.Size(), 0u);
}

TEST(Commands_PU, RemovePU_Undo_PURestored)
{
    ManifestEditorState state;
    AddPUCommand add(StringCRC("TestPU"));
    add.Execute(state);

    RemovePUCommand remove(StringCRC("TestPU"));
    remove.Execute(state);
    remove.Undo(state);

    ASSERT_EQ(state.manifest.processingUnits.Size(), 1u);
    EXPECT_TRUE(state.manifest.processingUnits[0].instanceId == StringCRC("TestPU"));
}

// ==============================================================================
// SetPUFrequencyCommand
// ==============================================================================

TEST(Commands_PU, SetPUFrequency_Execute_FreqChanged)
{
    ManifestEditorState state;
    AddPUCommand add(StringCRC("TestPU"), 30.0f);
    add.Execute(state);

    SetPUFrequencyCommand cmd(StringCRC("TestPU"), 60.0f);
    cmd.Execute(state);

    EXPECT_FLOAT_EQ(state.manifest.processingUnits[0].frequencyHz, 60.0f);
}

TEST(Commands_PU, SetPUFrequency_Undo_FreqRestored)
{
    ManifestEditorState state;
    AddPUCommand add(StringCRC("TestPU"), 30.0f);
    add.Execute(state);

    SetPUFrequencyCommand cmd(StringCRC("TestPU"), 60.0f);
    cmd.Execute(state);
    cmd.Undo(state);

    EXPECT_FLOAT_EQ(state.manifest.processingUnits[0].frequencyHz, 30.0f);
}

// ==============================================================================
// SetPUThreadCommand
// ==============================================================================

TEST(Commands_PU, SetPUThread_Execute_ThreadChanged)
{
    ManifestEditorState state;
    AddPUCommand add(StringCRC("TestPU"), 30.0f, false);
    add.Execute(state);

    SetPUThreadCommand cmd(StringCRC("TestPU"), true);
    cmd.Execute(state);

    EXPECT_TRUE(state.manifest.processingUnits[0].dedicatedThread);
}

TEST(Commands_PU, SetPUThread_Undo_ThreadRestored)
{
    ManifestEditorState state;
    AddPUCommand add(StringCRC("TestPU"), 30.0f, false);
    add.Execute(state);

    SetPUThreadCommand cmd(StringCRC("TestPU"), true);
    cmd.Execute(state);
    cmd.Undo(state);

    EXPECT_FALSE(state.manifest.processingUnits[0].dedicatedThread);
}

// ==============================================================================
// ReorderPUCommand
// ==============================================================================

TEST(Commands_PU, ReorderPU_Execute_OrderChanged)
{
    ManifestEditorState state;
    AddPUCommand addA(StringCRC("A"));
    addA.Execute(state);
    AddPUCommand addB(StringCRC("B"));
    addB.Execute(state);

    // Move "A" from index 0 to index 1 — "B" should end up at 0
    ReorderPUCommand cmd(StringCRC("A"), 1);
    cmd.Execute(state);

    ASSERT_EQ(state.manifest.processingUnits.Size(), 2u);
    EXPECT_TRUE(state.manifest.processingUnits[0].instanceId == StringCRC("B"));
    EXPECT_TRUE(state.manifest.processingUnits[1].instanceId == StringCRC("A"));
}

// ==============================================================================
// Dirty flag
// ==============================================================================

TEST(Commands_PU, MarksDirty_AfterExecute)
{
    ManifestEditorState state;
    state.MarkClean();

    AddPUCommand cmd(StringCRC("TestPU"));
    cmd.Execute(state);

    EXPECT_TRUE(state.isDirty);
}
