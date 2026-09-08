#include <gtest/gtest.h>
#include <DiaApplicationFlowEditor/V2/Commands/PUCommands.h>
#include <DiaApplicationFlowEditor/V2/Commands/ModuleCommands.h>
#include <DiaApplicationFlowEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

namespace
{
    void AddTestPU(ManifestEditorState& state, const char* puName)
    {
        StringCRC puCrc(puName);
        AddPUCommand addPUCmd(puCrc);
        addPUCmd.Execute(state);
    }
}

// ==============================================================================
// AddModuleCommand
// ==============================================================================

TEST(Commands_Module, AddModule_Execute_ModulePresent)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand cmd(StringCRC("MainPU"), StringCRC("RenderMod"), StringCRC("RenderModuleType"));
    cmd.Execute(state);

    ASSERT_EQ(state.manifest.processingUnits[0].modules.Size(), 1u);
    EXPECT_TRUE(state.manifest.processingUnits[0].modules[0].instanceId == StringCRC("RenderMod"));
}

TEST(Commands_Module, AddModule_Undo_ModuleGone)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand cmd(StringCRC("MainPU"), StringCRC("RenderMod"), StringCRC("RenderModuleType"));
    cmd.Execute(state);
    cmd.Undo(state);

    EXPECT_EQ(state.manifest.processingUnits[0].modules.Size(), 0u);
}

// ==============================================================================
// RemoveModuleCommand
// ==============================================================================

TEST(Commands_Module, RemoveModule_Execute_ModuleGone)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand add(StringCRC("MainPU"), StringCRC("RenderMod"), StringCRC("RenderModuleType"));
    add.Execute(state);

    RemoveModuleCommand remove(StringCRC("MainPU"), StringCRC("RenderMod"));
    remove.Execute(state);

    EXPECT_EQ(state.manifest.processingUnits[0].modules.Size(), 0u);
}

TEST(Commands_Module, RemoveModule_Undo_ModuleRestored)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand add(StringCRC("MainPU"), StringCRC("RenderMod"), StringCRC("RenderModuleType"));
    add.Execute(state);

    RemoveModuleCommand remove(StringCRC("MainPU"), StringCRC("RenderMod"));
    remove.Execute(state);
    remove.Undo(state);

    ASSERT_EQ(state.manifest.processingUnits[0].modules.Size(), 1u);
    EXPECT_TRUE(state.manifest.processingUnits[0].modules[0].instanceId == StringCRC("RenderMod"));
}

// ==============================================================================
// AddModuleDepCommand
// ==============================================================================

TEST(Commands_Module, AddModuleDep_Execute_DepPresent)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addA(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addA.Execute(state);
    AddModuleCommand addB(StringCRC("MainPU"), StringCRC("ModB"), StringCRC("TypeB"));
    addB.Execute(state);

    AddModuleDepCommand cmd(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("ModB"));
    cmd.Execute(state);

    auto& deps = state.manifest.processingUnits[0].modules[0].dependencies;
    ASSERT_EQ(deps.Size(), 1u);
    EXPECT_TRUE(deps[0] == StringCRC("ModB"));
}

TEST(Commands_Module, AddModuleDep_Undo_DepGone)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addA(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addA.Execute(state);

    AddModuleDepCommand cmd(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("ModB"));
    cmd.Execute(state);
    cmd.Undo(state);

    EXPECT_EQ(state.manifest.processingUnits[0].modules[0].dependencies.Size(), 0u);
}

// ==============================================================================
// RemoveModuleDepCommand
// ==============================================================================

TEST(Commands_Module, RemoveModuleDep_Execute_DepGone)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addA(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addA.Execute(state);

    AddModuleDepCommand addDep(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("ModB"));
    addDep.Execute(state);

    RemoveModuleDepCommand remove(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("ModB"));
    remove.Execute(state);

    EXPECT_EQ(state.manifest.processingUnits[0].modules[0].dependencies.Size(), 0u);
}

TEST(Commands_Module, RemoveModuleDep_Undo_DepRestored)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addA(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addA.Execute(state);

    AddModuleDepCommand addDep(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("ModB"));
    addDep.Execute(state);

    RemoveModuleDepCommand remove(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("ModB"));
    remove.Execute(state);
    remove.Undo(state);

    auto& deps = state.manifest.processingUnits[0].modules[0].dependencies;
    ASSERT_EQ(deps.Size(), 1u);
    EXPECT_TRUE(deps[0] == StringCRC("ModB"));
}

// ==============================================================================
// SetModuleStagesCommand
// ==============================================================================

TEST(Commands_Module, SetModuleStages_Execute_StagesChanged)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addMod(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addMod.Execute(state);

    StringCRC newStages[] = { StringCRC("Boot") };
    SetModuleStagesCommand cmd(StringCRC("MainPU"), StringCRC("ModA"), newStages, 1u);
    cmd.Execute(state);

    auto& stages = state.manifest.processingUnits[0].modules[0].stages;
    ASSERT_EQ(stages.Size(), 1u);
    EXPECT_TRUE(stages[0] == StringCRC("Boot"));
}

TEST(Commands_Module, SetModuleStages_Undo_StagesRestored)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addMod(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addMod.Execute(state);

    // Module starts with empty stages; set then undo.
    StringCRC newStages[] = { StringCRC("Boot") };
    SetModuleStagesCommand cmd(StringCRC("MainPU"), StringCRC("ModA"), newStages, 1u);
    cmd.Execute(state);
    cmd.Undo(state);

    EXPECT_EQ(state.manifest.processingUnits[0].modules[0].stages.Size(), 0u);
}

// ==============================================================================
// SetModuleStartTimeoutCommand
// ==============================================================================

TEST(Commands_Module, SetModuleStartTimeout_ExecuteUndo)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addMod(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addMod.Execute(state);
    // Default startTimeoutMs is 10000.f

    SetModuleStartTimeoutCommand cmd(StringCRC("MainPU"), StringCRC("ModA"), 5000.0f);
    cmd.Execute(state);
    EXPECT_FLOAT_EQ(state.manifest.processingUnits[0].modules[0].startTimeoutMs, 5000.0f);

    cmd.Undo(state);
    EXPECT_FLOAT_EQ(state.manifest.processingUnits[0].modules[0].startTimeoutMs, 10000.0f);
}

// ==============================================================================
// SetModuleStopTimeoutCommand
// ==============================================================================

TEST(Commands_Module, SetModuleStopTimeout_ExecuteUndo)
{
    ManifestEditorState state;
    AddTestPU(state, "MainPU");

    AddModuleCommand addMod(StringCRC("MainPU"), StringCRC("ModA"), StringCRC("TypeA"));
    addMod.Execute(state);
    // Default stopTimeoutMs is 5000.f

    SetModuleStopTimeoutCommand cmd(StringCRC("MainPU"), StringCRC("ModA"), 1000.0f);
    cmd.Execute(state);
    EXPECT_FLOAT_EQ(state.manifest.processingUnits[0].modules[0].stopTimeoutMs, 1000.0f);

    cmd.Undo(state);
    EXPECT_FLOAT_EQ(state.manifest.processingUnits[0].modules[0].stopTimeoutMs, 5000.0f);
}
