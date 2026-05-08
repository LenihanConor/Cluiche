#include <gtest/gtest.h>
#include <DiaEditor/Command/CommandHistory.h>
#include <DiaEditor/Command/IEditorCommand.h>

using namespace Dia::Editor;

namespace
{
    class CountingCommand : public IEditorCommand
    {
    public:
        int& executeCount;
        int& undoCount;
        CountingCommand(int& exec, int& undo) : executeCount(exec), undoCount(undo) {}
        void Execute() override { ++executeCount; }
        void Undo() override { ++undoCount; }
        const char* GetDescription() const override { return "CountingCommand"; }
    };
}

TEST(CommandHistory, ExecuteAndUndo)
{
    CommandHistory history;
    int execCount = 0, undoCount = 0;
    history.ExecuteCommand(new CountingCommand(execCount, undoCount));
    EXPECT_EQ(execCount, 1);
    EXPECT_TRUE(history.CanUndo());
    history.Undo();
    EXPECT_EQ(undoCount, 1);
    EXPECT_FALSE(history.CanUndo());
}

TEST(CommandHistory, UndoRedo)
{
    CommandHistory history;
    int execCount = 0, undoCount = 0;
    history.ExecuteCommand(new CountingCommand(execCount, undoCount));
    history.Undo();
    EXPECT_TRUE(history.CanRedo());
    history.Redo();
    EXPECT_EQ(execCount, 2);
    EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistory, NewCommandClearsRedo)
{
    CommandHistory history;
    int e1 = 0, u1 = 0, e2 = 0, u2 = 0;
    history.ExecuteCommand(new CountingCommand(e1, u1));
    history.Undo();
    EXPECT_TRUE(history.CanRedo());
    history.ExecuteCommand(new CountingCommand(e2, u2));
    EXPECT_FALSE(history.CanRedo());
}

TEST(CommandHistory, SavePoint)
{
    CommandHistory history;
    history.MarkSavePoint();
    EXPECT_TRUE(history.IsAtSavePoint());
    int e = 0, u = 0;
    history.ExecuteCommand(new CountingCommand(e, u));
    EXPECT_FALSE(history.IsAtSavePoint());
    history.Undo();
    EXPECT_TRUE(history.IsAtSavePoint());
}

TEST(CommandHistory, CompoundCommand)
{
    CommandHistory history;
    int e1 = 0, u1 = 0, e2 = 0, u2 = 0;
    history.BeginCompound();
    history.ExecuteCommand(new CountingCommand(e1, u1));
    history.ExecuteCommand(new CountingCommand(e2, u2));
    history.EndCompound();
    EXPECT_EQ(e1, 1);
    EXPECT_EQ(e2, 1);
    history.Undo();
    EXPECT_EQ(u1, 1);
    EXPECT_EQ(u2, 1);
    EXPECT_FALSE(history.CanUndo());
}
