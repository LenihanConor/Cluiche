// TestCommandHistory.cpp — Unit tests for CommandHistory and CompoundCommand
// Suite: CommandHistory

#include <gtest/gtest.h>

#include <DiaApplicationEditor/V2/ManifestEditorState.h>
#include <DiaApplicationEditor/V2/Commands/ICommand.h>
#include <DiaApplicationEditor/V2/Commands/CommandHistory.h>
#include <DiaApplicationEditor/V2/Commands/CompoundCommand.h>

using namespace Dia::ApplicationFlow::Editor;

// ==============================================================================
// TestCommand — concrete command for testing
// ==============================================================================

namespace
{
    struct TestCommand : public ICommand
    {
        explicit TestCommand(const char* desc = "TestCmd")
            : mDescription(desc)
            , mExecuteCount(0)
            , mUndoCount(0)
        {}

        void Execute(ManifestEditorState&) override { ++mExecuteCount; }
        void Undo(ManifestEditorState&) override    { ++mUndoCount; }
        const char* GetDescription() const override { return mDescription; }

        const char* mDescription;
        int mExecuteCount;
        int mUndoCount;
    };
} // anonymous namespace

// ==============================================================================
// CommandHistory tests
// ==============================================================================

TEST(CommandHistory, Execute_CanUndo)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);

    EXPECT_TRUE(history.CanUndo());
}

TEST(CommandHistory, Execute_CannotRedo_Initially)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);

    EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistory, Undo_ThenRedo_RestoresState)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);
    history.Undo(doc);

    EXPECT_TRUE(history.CanRedo());

    history.Redo(doc);

    EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistory, Execute_AfterUndo_ClearsRedoStack)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);
    history.Execute(new TestCommand("B"), doc);
    history.Undo(doc);
    history.Execute(new TestCommand("C"), doc);

    EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistory, CapAt100_OldestDropped)
{
    CommandHistory history;
    ManifestEditorState doc;

    for (int i = 0; i < 101; ++i)
    {
        history.Execute(new TestCommand("cmd"), doc);
    }

    EXPECT_EQ(history.GetCount(), 100u);
}

TEST(CommandHistory, Clear_EmptiesHistory)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);
    history.Execute(new TestCommand("B"), doc);
    history.Clear();

    EXPECT_FALSE(history.CanUndo());
    EXPECT_EQ(history.GetCount(), 0u);
}

TEST(CommandHistory, SetSavePoint_AtSavePoint_True)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);
    history.SetSavePoint();

    EXPECT_TRUE(history.IsAtSavePoint());
}

TEST(CommandHistory, SetSavePoint_AfterEdit_NotAtSavePoint)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);
    history.SetSavePoint();
    history.Execute(new TestCommand("B"), doc);

    EXPECT_FALSE(history.IsAtSavePoint());
}

TEST(CommandHistory, SetSavePoint_UndoToSavePoint_IsAtSavePoint)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);
    history.SetSavePoint();
    history.Execute(new TestCommand("B"), doc);
    history.Undo(doc);

    EXPECT_TRUE(history.IsAtSavePoint());
}

TEST(CommandHistory, CompoundCommand_ExecutesAllChildren)
{
    ManifestEditorState doc;

    auto* child1 = new TestCommand("C1");
    auto* child2 = new TestCommand("C2");

    auto* compound = new CompoundCommand();
    compound->Add(child1);
    compound->Add(child2);

    CommandHistory history;
    history.Execute(compound, doc);

    EXPECT_EQ(child1->mExecuteCount, 1);
    EXPECT_EQ(child2->mExecuteCount, 1);
}

TEST(CommandHistory, CompoundCommand_UndoReverseOrder)
{
    ManifestEditorState doc;

    // Track call order via a shared counter.
    static int sCallOrder[4];
    static int sCallIdx = 0;

    struct OrderedCommand : public ICommand
    {
        explicit OrderedCommand(int id) : mId(id) {}
        void Execute(ManifestEditorState&) override {}
        void Undo(ManifestEditorState&) override
        {
            sCallOrder[sCallIdx++] = mId;
        }
        const char* GetDescription() const override { return "ordered"; }
        int mId;
    };

    sCallIdx = 0;

    auto* compound = new CompoundCommand();
    compound->Add(new OrderedCommand(1));
    compound->Add(new OrderedCommand(2));

    CommandHistory history;
    history.Execute(compound, doc);
    history.Undo(doc);

    // Undo should call child2 then child1 (reverse).
    EXPECT_EQ(sCallOrder[0], 2);
    EXPECT_EQ(sCallOrder[1], 1);
}

TEST(CommandHistory, GetCommand_ValidIndex_ReturnsCommand)
{
    CommandHistory history;
    ManifestEditorState doc;

    history.Execute(new TestCommand("A"), doc);

    EXPECT_NE(history.GetCommand(0u), nullptr);
}
