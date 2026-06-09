// TestCommands_Stream.cpp — Unit tests for stream commands
// Suite: Commands_Stream

#include <gtest/gtest.h>

#include <DiaApplicationFlowEditor/V2/Commands/StreamCommands.h>
#include <DiaApplicationFlowEditor/V2/ManifestEditorState.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>

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

    void AddTestStream(ManifestEditorState& doc, const char* idStr)
    {
        StreamDeclaration s;
        s.id          = StringCRC(idStr);
        s.kind        = StringCRC("EventStream");
        s.payloadType = StringCRC("TestPayload");
        doc.manifest.streams.Add(s);
    }
} // anonymous namespace

// -------------------------------------------------------------------------
// AddStreamCommand
// -------------------------------------------------------------------------

TEST(Commands_Stream, AddStream_Execute_StreamPresent)
{
    ManifestEditorState doc = MakeDoc();

    AddStreamCommand cmd("myStream", StringCRC("EventStream"), StringCRC("InputEvent"));
    cmd.Execute(doc);

    ASSERT_EQ(doc.manifest.streams.Size(), 1u);
    EXPECT_EQ(doc.manifest.streams[0u].id, StringCRC("myStream"));
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stream, AddStream_Undo_StreamGone)
{
    ManifestEditorState doc = MakeDoc();

    AddStreamCommand cmd("myStream", StringCRC("EventStream"), StringCRC("InputEvent"));
    cmd.Execute(doc);
    doc.MarkClean();
    cmd.Undo(doc);

    EXPECT_EQ(doc.manifest.streams.Size(), 0u);
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stream, AddStream_ReservedPrefix_NoEffect)
{
    ManifestEditorState doc = MakeDoc();

    AddStreamCommand cmd("$internal", StringCRC("EventStream"), StringCRC("InputEvent"));
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.streams.Size(), 0u);
    EXPECT_FALSE(doc.isDirty);
}

// -------------------------------------------------------------------------
// RemoveStreamCommand
// -------------------------------------------------------------------------

TEST(Commands_Stream, RemoveStream_Execute_StreamGone)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStream(doc, "streamA");

    RemoveStreamCommand cmd(StringCRC("streamA"));
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.streams.Size(), 0u);
    EXPECT_TRUE(doc.isDirty);
}

TEST(Commands_Stream, RemoveStream_Undo_StreamRestored)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStream(doc, "streamA");

    RemoveStreamCommand cmd(StringCRC("streamA"));
    cmd.Execute(doc);
    doc.MarkClean();
    cmd.Undo(doc);

    ASSERT_EQ(doc.manifest.streams.Size(), 1u);
    EXPECT_EQ(doc.manifest.streams[0u].id, StringCRC("streamA"));
    EXPECT_TRUE(doc.isDirty);
}

// -------------------------------------------------------------------------
// SetStreamKindCommand
// -------------------------------------------------------------------------

TEST(Commands_Stream, SetStreamKind_ExecuteUndo)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStream(doc, "streamA");

    SetStreamKindCommand cmd(StringCRC("streamA"), StringCRC("FrameStream"));
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.streams[0u].kind, StringCRC("FrameStream"));

    cmd.Undo(doc);

    EXPECT_EQ(doc.manifest.streams[0u].kind, StringCRC("EventStream"));
}

// -------------------------------------------------------------------------
// SetStreamPayloadCommand
// -------------------------------------------------------------------------

TEST(Commands_Stream, SetStreamPayload_ExecuteUndo)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStream(doc, "streamA");

    SetStreamPayloadCommand cmd(StringCRC("streamA"), StringCRC("PhysicsEvent"));
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.streams[0u].payloadType, StringCRC("PhysicsEvent"));

    cmd.Undo(doc);

    EXPECT_EQ(doc.manifest.streams[0u].payloadType, StringCRC("TestPayload"));
}

// -------------------------------------------------------------------------
// SetStreamCapacityCommand
// -------------------------------------------------------------------------

TEST(Commands_Stream, SetStreamCapacity_ExecuteUndo)
{
    ManifestEditorState doc = MakeDoc();
    AddTestStream(doc, "streamA");
    doc.manifest.streams[0u].capacity = 8u;

    SetStreamCapacityCommand cmd(StringCRC("streamA"), 32u);
    cmd.Execute(doc);

    EXPECT_EQ(doc.manifest.streams[0u].capacity, 32u);

    cmd.Undo(doc);

    EXPECT_EQ(doc.manifest.streams[0u].capacity, 8u);
}
