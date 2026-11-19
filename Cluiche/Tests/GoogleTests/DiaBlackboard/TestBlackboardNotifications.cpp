#include <gtest/gtest.h>

#include <DiaBlackboard/Blackboard.h>
#include <DiaBlackboard/Testing/BlackboardTestHelpers.h>

using namespace Dia::Blackboard;
using Dia::Blackboard::Testing::MockBlackboardObserver;

namespace
{
    // Does NOT override OnSlotChanged.
    class LegacyObserver : public IBlackboardObserver
    {
    public:
        int registered = 0;
        int unregistered = 0;
        Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Legacy"); }
        void OnSlotRegistered(Dia::Core::StringCRC) override { ++registered; }
        void OnSlotUnregistered(Dia::Core::StringCRC) override { ++unregistered; }
    };
}

TEST(BlackboardNotifications, Set_WritesValueAndNotifiesOnce)
{
    Blackboard board;
    MockBlackboardObserver obs;
    board.Register<int>(Dia::Core::StringCRC("Hp"));
    board.AddObserver(obs);
    board.Set<int>(Dia::Core::StringCRC("Hp"), 42);
    EXPECT_EQ(42, board.Get<int>(Dia::Core::StringCRC("Hp")));
    EXPECT_EQ(1, obs.changeCount);
    EXPECT_TRUE(obs.lastChanged == Dia::Core::StringCRC("Hp"));
    board.RemoveObserver(obs);
}

TEST(BlackboardNotifications, Set_TwoObservers_EachNotifiedOnce)
{
    Blackboard board;
    MockBlackboardObserver a, b;
    board.Register<int>(Dia::Core::StringCRC("Hp"));
    board.AddObserver(a);
    board.AddObserver(b);
    board.Set<int>(Dia::Core::StringCRC("Hp"), 1);
    EXPECT_EQ(1, a.changeCount);
    EXPECT_EQ(1, b.changeCount);
    board.RemoveObserver(a);
    board.RemoveObserver(b);
}

TEST(BlackboardNotifications, Set_DoesNotNotifyParentOrChildObservers)
{
    Blackboard parent;
    Blackboard child;
    MockBlackboardObserver parentObs, childObs, selfObs;
    ASSERT_TRUE(child.SetParent(&parent));
    parent.Register<int>(Dia::Core::StringCRC("Hp"));
    child.Register<int>(Dia::Core::StringCRC("Hp"));
    parent.AddObserver(parentObs);
    child.AddObserver(childObs);

    // Set on a third board sharing the key must not touch either.
    Blackboard other;
    other.Register<int>(Dia::Core::StringCRC("Hp"));
    other.AddObserver(selfObs);
    other.Set<int>(Dia::Core::StringCRC("Hp"), 5);
    EXPECT_EQ(1, selfObs.changeCount);
    EXPECT_EQ(0, parentObs.changeCount);
    EXPECT_EQ(0, childObs.changeCount);

    // Set on child notifies child only, not parent.
    child.Set<int>(Dia::Core::StringCRC("Hp"), 7);
    EXPECT_EQ(1, childObs.changeCount);
    EXPECT_EQ(0, parentObs.changeCount);

    // Set on parent notifies parent only, not child.
    parent.Set<int>(Dia::Core::StringCRC("Hp"), 9);
    EXPECT_EQ(1, parentObs.changeCount);
    EXPECT_EQ(1, childObs.changeCount);

    parent.RemoveObserver(parentObs);
    child.RemoveObserver(childObs);
    other.RemoveObserver(selfObs);
}

TEST(BlackboardNotifications, GetReferenceWrite_DoesNotNotify)
{
    Blackboard board;
    MockBlackboardObserver obs;
    board.Register<int>(Dia::Core::StringCRC("Hp"));
    board.AddObserver(obs);
    board.Get<int>(Dia::Core::StringCRC("Hp")) = 3;
    *board.TryGet<int>(Dia::Core::StringCRC("Hp")) = 4;
    EXPECT_EQ(0, obs.changeCount);
    board.RemoveObserver(obs);
}

TEST(BlackboardNotifications, ObserverWithoutOnSlotChanged_BehavesAsBefore)
{
    Blackboard board;
    LegacyObserver obs;
    board.AddObserver(obs);
    board.Register<int>(Dia::Core::StringCRC("Hp"));
    board.Set<int>(Dia::Core::StringCRC("Hp"), 1);
    board.Unregister(Dia::Core::StringCRC("Hp"));
    EXPECT_EQ(1, obs.registered);
    EXPECT_EQ(1, obs.unregistered);
    board.RemoveObserver(obs);
}

#ifdef _DEBUG
TEST(BlackboardNotifications, Set_KeyOnlyInParent_Asserts)
{
    Blackboard parent;
    Blackboard child;
    ASSERT_TRUE(child.SetParent(&parent));
    parent.Register<int>(Dia::Core::StringCRC("Hp"));
    EXPECT_DEATH(child.Set<int>(Dia::Core::StringCRC("Hp"), 1), "");
}

TEST(BlackboardNotifications, Set_TypeMismatch_Asserts)
{
    Blackboard board;
    board.Register<int>(Dia::Core::StringCRC("Hp"));
    EXPECT_DEATH(board.Set<float>(Dia::Core::StringCRC("Hp"), 1.0f), "");
}

TEST(BlackboardNotifications, NinthObserver_Asserts)
{
    Blackboard board;
    MockBlackboardObserver obs[9];
    for (int i = 0; i < 8; ++i) board.AddObserver(obs[i]);
    EXPECT_DEATH(board.AddObserver(obs[8]), "");
    for (int i = 0; i < 8; ++i) board.RemoveObserver(obs[i]);
}

TEST(BlackboardNotifications, Set_UnregisteredKey_Asserts)
{
    Blackboard board;
    EXPECT_DEATH(board.Set<int>(Dia::Core::StringCRC("Missing"), 1), "");
}
#endif

// -------------------------------------------------------------------------
// Edge tests (gap audit, task 10)
// -------------------------------------------------------------------------

namespace
{
    // Calls Set on a second key from inside OnSlotChanged.
    class ReentrantObserver : public IBlackboardObserver
    {
    public:
        Blackboard* board = nullptr;
        int changes = 0;
        Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Reentrant"); }
        void OnSlotRegistered(Dia::Core::StringCRC) override {}
        void OnSlotUnregistered(Dia::Core::StringCRC) override {}
        void OnSlotChanged(Dia::Core::StringCRC key) override
        {
            ++changes;
            if (key == Dia::Core::StringCRC("A"))
            {
                board->Set<int>(Dia::Core::StringCRC("B"), 2);
            }
        }
    };

    // Removes itself from the board when notified.
    class SelfRemovingObserver : public IBlackboardObserver
    {
    public:
        Blackboard* board = nullptr;
        int changes = 0;
        Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("SelfRemoving"); }
        void OnSlotRegistered(Dia::Core::StringCRC) override {}
        void OnSlotUnregistered(Dia::Core::StringCRC) override {}
        void OnSlotChanged(Dia::Core::StringCRC) override
        {
            ++changes;
            board->RemoveObserver(*this);
        }
    };
}

TEST(BlackboardNotifications, Set_InsideObserverCallback_NestedSetNotifies)
{
    Blackboard board;
    ReentrantObserver obs;
    obs.board = &board;
    board.Register<int>(Dia::Core::StringCRC("A"));
    board.Register<int>(Dia::Core::StringCRC("B"));
    board.AddObserver(obs);
    board.Set<int>(Dia::Core::StringCRC("A"), 1);
    EXPECT_EQ(2, obs.changes);
    EXPECT_EQ(2, board.Get<int>(Dia::Core::StringCRC("B")));
    board.RemoveObserver(obs);
}

TEST(BlackboardNotifications, ObserverRemovedDuringCallback_NoCrashAndNotNotifiedAgain)
{
    Blackboard board;
    SelfRemovingObserver obs;
    obs.board = &board;
    board.Register<int>(Dia::Core::StringCRC("A"));
    board.AddObserver(obs);
    board.Set<int>(Dia::Core::StringCRC("A"), 1);
    EXPECT_EQ(1, obs.changes);
    board.Set<int>(Dia::Core::StringCRC("A"), 2);
    EXPECT_EQ(1, obs.changes);
}

TEST(BlackboardNotifications, Set_EqualValue_StillNotifies)
{
    Blackboard board;
    MockBlackboardObserver obs;
    board.Register<int>(Dia::Core::StringCRC("Hp"));
    board.AddObserver(obs);
    board.Set<int>(Dia::Core::StringCRC("Hp"), 0);
    EXPECT_EQ(1, obs.changeCount);
    board.RemoveObserver(obs);
}

TEST(BlackboardNotifications, Set_AfterObserverRemoved_NotNotified)
{
    Blackboard board;
    MockBlackboardObserver a, b;
    board.Register<int>(Dia::Core::StringCRC("Hp"));
    board.AddObserver(a);
    board.AddObserver(b);
    board.RemoveObserver(a);
    board.Set<int>(Dia::Core::StringCRC("Hp"), 1);
    EXPECT_EQ(0, a.changeCount);
    EXPECT_EQ(1, b.changeCount);
    board.RemoveObserver(b);
}
