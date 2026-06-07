// TestManifestEditorState.cpp - Unit tests for ManifestEditorState
//
// Tests the editor's in-memory document state: dirty tracking,
// file path, manifest version, and module provenance.

#include <gtest/gtest.h>
#include <DiaApplicationEditor/V2/ManifestEditorState.h>

using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::Core;

// ==============================================================================
// Dirty flag and construction
// ==============================================================================

TEST(ManifestEditorState, DefaultConstruct_IsDirtyFalse)
{
    ManifestEditorState state;
    EXPECT_FALSE(state.isDirty);
}

TEST(ManifestEditorState, DefaultConstruct_HasManifestFalse)
{
    ManifestEditorState state;
    EXPECT_FALSE(state.hasManifest);
}

TEST(ManifestEditorState, DefaultConstruct_FilePathEmpty)
{
    ManifestEditorState state;
    EXPECT_EQ(state.filePath[0], '\0');
}

TEST(ManifestEditorState, MarkDirty_SetsDirtyTrue)
{
    ManifestEditorState state;
    state.MarkDirty();
    EXPECT_TRUE(state.isDirty);
}

TEST(ManifestEditorState, MarkClean_ClearsDirty)
{
    ManifestEditorState state;
    state.MarkDirty();
    state.MarkClean();
    EXPECT_FALSE(state.isDirty);
}

// ==============================================================================
// Provenance — empty state
// ==============================================================================

TEST(ManifestEditorState, FindProvenance_EmptyState_ReturnsNull)
{
    ManifestEditorState state;
    const ModuleProvenance* result = state.FindProvenance(
        StringCRC("SimPU"), StringCRC("DummyLevel"));
    EXPECT_EQ(result, nullptr);
}

// ==============================================================================
// Provenance — set and find
// ==============================================================================

TEST(ManifestEditorState, SetProvenance_ThenFind_ReturnsEntry)
{
    ManifestEditorState state;
    state.SetProvenance(StringCRC("SimPU"), StringCRC("DummyLevel"), "dummy.diaapp");

    const ModuleProvenance* result = state.FindProvenance(
        StringCRC("SimPU"), StringCRC("DummyLevel"));

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result->sourceFile.AsCStr(), "dummy.diaapp");
}

TEST(ManifestEditorState, SetProvenance_UpdateExisting_OverwritesSourceFile)
{
    ManifestEditorState state;
    state.SetProvenance(StringCRC("SimPU"), StringCRC("DummyLevel"), "first.diaapp");
    state.SetProvenance(StringCRC("SimPU"), StringCRC("DummyLevel"), "second.diaapp");

    const ModuleProvenance* result = state.FindProvenance(
        StringCRC("SimPU"), StringCRC("DummyLevel"));

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result->sourceFile.AsCStr(), "second.diaapp");
}

TEST(ManifestEditorState, SetProvenance_UpdateExisting_DoesNotAddDuplicate)
{
    ManifestEditorState state;
    state.SetProvenance(StringCRC("SimPU"), StringCRC("DummyLevel"), "first.diaapp");
    state.SetProvenance(StringCRC("SimPU"), StringCRC("DummyLevel"), "second.diaapp");

    EXPECT_EQ(state.provenance.Size(), 1u);
}

// ==============================================================================
// Provenance — lookup mismatches
// ==============================================================================

TEST(ManifestEditorState, FindProvenance_WrongPU_ReturnsNull)
{
    ManifestEditorState state;
    state.SetProvenance(StringCRC("SimPU"), StringCRC("DummyLevel"), "dummy.diaapp");

    const ModuleProvenance* result = state.FindProvenance(
        StringCRC("MainPU"), StringCRC("DummyLevel"));

    EXPECT_EQ(result, nullptr);
}

// ==============================================================================
// Manifest field defaults
// ==============================================================================

TEST(ManifestEditorState, ManifestField_DefaultVersion_IsThree)
{
    ManifestEditorState state;
    EXPECT_EQ(state.manifest.version, 3);
}
