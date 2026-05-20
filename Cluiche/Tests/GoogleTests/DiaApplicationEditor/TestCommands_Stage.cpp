// TestCommands_Stage.cpp — Unit tests for stage commands
// Suite: Commands_Stage

#include <gtest/gtest.h>

#include <DiaApplicationEditor/V2/Commands/StageCommands.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::ApplicationFlow;
using namespace Dia::Core;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

namespace
{
    ManifestEditorState MakeDoc()
    {
        ManifestEditorState doc;
        doc.hasManifest = true;
        return doc;
    }

    void AddTestStage(ManifestEditorState& doc, const char* name, const char* path = "")
    {
        StageDeclaration s;
        s.name         = StringCRC(name);
        s.manifestPath = path;
        doc.manifest.stages.Add(s);
    }
} // anonymous namespace

// -------------------------------------------------------------------------
// AddStageCommand
// -------------------------------------------------------------------------

TEST(Commands_Stage, AddStage_Execute_StagePresent)
{
    ManifestEditorState doc = MakeDoc();

    AddStageCommand cmd(StringCRC("MainMenu"), "main_menu.diastage");
    cmd.Execute(doc);

    ASSERT_EQ(doc.manifest.stages.Size(), 1u);
    EXPECT_EQ(doc.manifest.stages[0u].name, StringCRC("MainMenu"));
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stage, AddStage_Undo_StageGone)
{
    ManifestEditorState doc = MakeDoc();

    AddStageCommand cmd(StringCRC("MainMenu"), "main_menu.diastage");
    cmd.Execute(doc);
    doc.MarkClean();
    cmd.Undo(doc);

    EXPECT_EQ(doc.manifest.stages.Size(), 0u);
    EXPECT_TRUE(doc.isDirty);
}

// -------------------------------------------------------------------------
// RemoveStageCommand
// -------------------------------------------------------------------------

TEST(Commands_Stage, RemoveStage_Execute_StageGone)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "Gameplay");

    RemoveStageCommand cmd(StringCRC("Gameplay"));
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.stages.Size(), 0u);
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stage, RemoveStage_Undo_StageRestored)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "Gameplay");

    RemoveStageCommand cmd(StringCRC("Gameplay"));
    cmd.Execute(doc);
    doc.MarkClean();
    cmd.Undo(doc);

    ASSERT_EQ(doc.manifest.stages.Size(), 1u);
    EXPECT_EQ(doc.manifest.stages[0u].name, StringCRC("Gameplay"));
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stage, RemoveStage_ClearsInitialStage_WhenInitial)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "Gameplay");
    doc.manifest.initialStage = StringCRC("Gameplay");

    RemoveStageCommand cmd(StringCRC("Gameplay"));
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.initialStage, StringCRC());
}

// -------------------------------------------------------------------------
// SetStageTriggerCommand
// -------------------------------------------------------------------------

TEST(Commands_Stage, SetStageTrigger_Execute_AutoChanged)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "Intro");

    SetStageTriggerCommand cmd(StringCRC("Intro"), true);
    cmd.Execute(doc);

    ASSERT_EQ(doc.manifest.autoStages.Size(), 1u);
    EXPECT_EQ(doc.manifest.autoStages[0u], StringCRC("Intro"));
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stage, SetStageTrigger_Undo_AutoRestored)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "Intro");

    SetStageTriggerCommand cmd(StringCRC("Intro"), true);
    cmd.Execute(doc);
    doc.MarkClean();
    cmd.Undo(doc);

    EXPECT_EQ(doc.manifest.autoStages.Size(), 0u);
    EXPECT_TRUE(doc.isDirty);
}

// -------------------------------------------------------------------------
// SetInitialStageCommand
// -------------------------------------------------------------------------

TEST(Commands_Stage, SetInitialStage_Execute_InitialChanged)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "Boot");
    doc.manifest.initialStage = StringCRC("OldStage");

    SetInitialStageCommand cmd(StringCRC("Boot"));
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.initialStage, StringCRC("Boot"));
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stage, SetInitialStage_Undo_InitialRestored)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "Boot");
    doc.manifest.initialStage = StringCRC("OldStage");

    SetInitialStageCommand cmd(StringCRC("Boot"));
    cmd.Execute(doc);
    doc.MarkClean();
    cmd.Undo(doc);

    EXPECT_EQ(doc.manifest.initialStage, StringCRC("OldStage"));
    EXPECT_TRUE(doc.isDirty);
}

// -------------------------------------------------------------------------
// ReorderStageCommand
// -------------------------------------------------------------------------

TEST(Commands_Stage, ReorderStage_Execute_OrderChanged)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStage(doc, "StageA");
    AddTestStage(doc, "StageB");
    AddTestStage(doc, "StageC");

    // Move StageC (index 2) to index 0
    ReorderStageCommand cmd(StringCRC("StageC"), 0);
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.stages[0u].name, StringCRC("StageC"));
    EXPECT_EQ(doc.manifest.stages[1u].name, StringCRC("StageA"));
    EXPECT_EQ(doc.manifest.stages[2u].name, StringCRC("StageB"));
    EXPECT_TRUE(doc.isDirty);
}
