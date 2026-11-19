#include <gtest/gtest.h>

#include <stdio.h>
#include <DiaBlackboard/Blackboard.h>

using namespace Dia::Blackboard;

namespace
{
    struct ScopeSlot
    {
        int mValue = 0;
    };

    void RegisterN(Blackboard& board, unsigned int count)
    {
        char keyBuf[16];
        for (unsigned int i = 0; i < count; ++i)
        {
            snprintf(keyBuf, sizeof(keyBuf), "Slot%u", i);
            board.Register<ScopeSlot>(Dia::Core::StringCRC(keyBuf));
        }
    }
}

TEST(BlackboardScopes, CustomCap_HoldsExactlyMaxSlots)
{
    Blackboard board(4);
    RegisterN(board, 4);
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Slot0")));
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Slot3")));
}

TEST(BlackboardScopes, DefaultCap_Holds32)
{
    Blackboard board;
    RegisterN(board, 32);
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Slot31")));
}

TEST(BlackboardScopes, GetName_ReturnsGivenName)
{
    Blackboard board(8, Dia::Core::StringCRC("Squad"));
    EXPECT_EQ(board.GetName(), Dia::Core::StringCRC("Squad"));
}

TEST(BlackboardScopes, GetName_DefaultIsDefaultStringCRC)
{
    Blackboard board;
    EXPECT_EQ(board.GetName(), Dia::Core::StringCRC());
}

TEST(BlackboardScopes, Unregister_FreesSlotForReuse)
{
    Blackboard board(2);
    board.Register<ScopeSlot>(Dia::Core::StringCRC("A"));
    board.Register<ScopeSlot>(Dia::Core::StringCRC("B"));
    board.Unregister(Dia::Core::StringCRC("A"));
    board.Register<ScopeSlot>(Dia::Core::StringCRC("C"));
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("B")));
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("C")));
    EXPECT_FALSE(board.Has(Dia::Core::StringCRC("A")));
}

#ifdef _DEBUG

TEST(BlackboardScopes, CustomCap_ExceedingAsserts)
{
    Blackboard board(4);
    RegisterN(board, 4);
    EXPECT_DEATH(board.Register<ScopeSlot>(Dia::Core::StringCRC("Overflow")), "");
}

TEST(BlackboardScopes, DefaultCap_33rdAsserts)
{
    Blackboard board;
    RegisterN(board, 32);
    EXPECT_DEATH(board.Register<ScopeSlot>(Dia::Core::StringCRC("Overflow")), "");
}

TEST(BlackboardScopes, ZeroCap_FirstRegisterAsserts)
{
    Blackboard board(0);
    EXPECT_DEATH(board.Register<ScopeSlot>(Dia::Core::StringCRC("Any")), "");
}

#endif // _DEBUG

// -------------------------------------------------------------------------
// Parent chain (task 4)
// -------------------------------------------------------------------------

TEST(BlackboardScopes, Parent_DefaultIsNull)
{
    Blackboard a;
    EXPECT_EQ(a.GetParent(), nullptr);
}

TEST(BlackboardScopes, SetParent_Valid_Succeeds)
{
    Blackboard parent;
    Blackboard child;
    EXPECT_TRUE(child.SetParent(&parent));
    EXPECT_EQ(child.GetParent(), &parent);
}

TEST(BlackboardScopes, SetParent_Self_Refused)
{
    Blackboard a;
    EXPECT_FALSE(a.SetParent(&a));
    EXPECT_EQ(a.GetParent(), nullptr);
}

TEST(BlackboardScopes, SetParent_TwoCycle_Refused)
{
    Blackboard a;
    Blackboard b;
    ASSERT_TRUE(b.SetParent(&a));
    EXPECT_FALSE(a.SetParent(&b));
    EXPECT_EQ(a.GetParent(), nullptr);
    EXPECT_EQ(b.GetParent(), &a);
}

TEST(BlackboardScopes, SetParent_LongCycle_Refused)
{
    Blackboard a, b, c, d;
    ASSERT_TRUE(b.SetParent(&a));
    ASSERT_TRUE(c.SetParent(&b));
    ASSERT_TRUE(d.SetParent(&c));
    EXPECT_FALSE(a.SetParent(&d));
    EXPECT_EQ(a.GetParent(), nullptr);
}

TEST(BlackboardScopes, SetParent_ChainOf8_Accepted_9thRefused)
{
    Blackboard* boards[9];
    for (int i = 0; i < 9; ++i) boards[i] = new Blackboard();
    bool ok = true;
    for (int i = 1; i < 8; ++i) ok = ok && boards[i]->SetParent(boards[i - 1]);
    EXPECT_TRUE(ok);
    EXPECT_FALSE(boards[8]->SetParent(boards[7]));
    EXPECT_EQ(boards[8]->GetParent(), nullptr);
    for (int i = 8; i >= 0; --i) delete boards[i];
}

TEST(BlackboardScopes, SetParent_RefusedCall_LeavesParentAndCountersUnchanged)
{
    Blackboard a;
    Blackboard b;
    Blackboard c;
    ASSERT_TRUE(b.SetParent(&a));
    // c -> b -> a ; refused: a -> c
    ASSERT_TRUE(c.SetParent(&b));
    EXPECT_FALSE(a.SetParent(&c));
    EXPECT_EQ(a.GetParent(), nullptr);
    EXPECT_EQ(b.GetParent(), &a);
    EXPECT_EQ(c.GetParent(), &b);
    // counters intact: detach properly and everything destroys cleanly
    c.SetParent(nullptr);
    b.SetParent(nullptr);
}

TEST(BlackboardScopes, SetParent_SameParentAgain_Succeeds)
{
    Blackboard parent;
    Blackboard child;
    ASSERT_TRUE(child.SetParent(&parent));
    EXPECT_TRUE(child.SetParent(&parent));
    EXPECT_EQ(child.GetParent(), &parent);
    // single detach must bring count to zero (destructor assert would fire otherwise)
    EXPECT_TRUE(child.SetParent(nullptr));
}

TEST(BlackboardScopes, SetParent_Null_Detaches)
{
    Blackboard* parent = new Blackboard();
    Blackboard child;
    ASSERT_TRUE(child.SetParent(parent));
    EXPECT_TRUE(child.SetParent(nullptr));
    EXPECT_EQ(child.GetParent(), nullptr);
    delete parent; // clean: no live children
}

TEST(BlackboardScopes, SetParent_Reparent_MovesChildCount)
{
    Blackboard* oldParent = new Blackboard();
    Blackboard newParent;
    Blackboard child;
    ASSERT_TRUE(child.SetParent(oldParent));
    ASSERT_TRUE(child.SetParent(&newParent));
    EXPECT_EQ(child.GetParent(), &newParent);
    delete oldParent; // clean
    child.SetParent(nullptr);
}

TEST(BlackboardScopes, Destroy_ChildFirst_IsClean)
{
    Blackboard parent;
    {
        Blackboard child;
        ASSERT_TRUE(child.SetParent(&parent));
    }
    // parent destroyed at scope end with zero children
}

TEST(BlackboardScopes, Get_StaysLocalOnly)
{
    Blackboard parent;
    Blackboard child;
    parent.Register<ScopeSlot>(Dia::Core::StringCRC("P"));
    ASSERT_TRUE(child.SetParent(&parent));
    EXPECT_FALSE(child.Has(Dia::Core::StringCRC("P")));
    EXPECT_EQ(child.TryGet<ScopeSlot>(Dia::Core::StringCRC("P")), nullptr);
}

#ifdef _DEBUG

TEST(BlackboardScopes, Destroy_ParentWithLiveChild_Asserts)
{
    Blackboard child;
    EXPECT_DEATH(
        {
            Blackboard* parent = new Blackboard();
            child.SetParent(parent);
            delete parent;
        }, "");
}

#endif // _DEBUG

// -------------------------------------------------------------------------
// Chain reads (task 5)
// -------------------------------------------------------------------------

namespace
{
    struct ChainFixture
    {
        Blackboard grand;
        Blackboard parent;
        Blackboard child;
        ChainFixture()
        {
            parent.SetParent(&grand);
            child.SetParent(&parent);
        }
        ~ChainFixture()
        {
            child.SetParent(nullptr);
            parent.SetParent(nullptr);
        }
    };
}

TEST(BlackboardScopes, Lookup_FindsLocal)
{
    ChainFixture f;
    f.child.Register<ScopeSlot>(Dia::Core::StringCRC("K")).mValue = 1;
    const Blackboard& c = f.child;
    const ScopeSlot* s = c.Lookup<ScopeSlot>(Dia::Core::StringCRC("K"));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->mValue, 1);
}

TEST(BlackboardScopes, Lookup_FindsInParent)
{
    ChainFixture f;
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("K")).mValue = 2;
    const ScopeSlot* s = f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K"));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->mValue, 2);
}

TEST(BlackboardScopes, Lookup_FindsInGrandparent)
{
    ChainFixture f;
    f.grand.Register<ScopeSlot>(Dia::Core::StringCRC("K")).mValue = 3;
    const ScopeSlot* s = f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K"));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->mValue, 3);
}

TEST(BlackboardScopes, Lookup_LocalShadowsParent)
{
    ChainFixture f;
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("K")).mValue = 2;
    f.child.Register<ScopeSlot>(Dia::Core::StringCRC("K")).mValue = 1;
    const ScopeSlot* s = f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K"));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->mValue, 1);
    EXPECT_EQ(f.child.FindOwner(Dia::Core::StringCRC("K")), &f.child);
}

TEST(BlackboardScopes, Lookup_Missing_ReturnsNullFalseNull)
{
    ChainFixture f;
    EXPECT_EQ(f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("Nope")), nullptr);
    EXPECT_FALSE(f.child.HasInChain(Dia::Core::StringCRC("Nope")));
    EXPECT_EQ(f.child.FindOwner(Dia::Core::StringCRC("Nope")), nullptr);
}

TEST(BlackboardScopes, HasInChain_LocalParentGrandparent)
{
    ChainFixture f;
    f.child.Register<ScopeSlot>(Dia::Core::StringCRC("L"));
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("P"));
    f.grand.Register<ScopeSlot>(Dia::Core::StringCRC("G"));
    EXPECT_TRUE(f.child.HasInChain(Dia::Core::StringCRC("L")));
    EXPECT_TRUE(f.child.HasInChain(Dia::Core::StringCRC("P")));
    EXPECT_TRUE(f.child.HasInChain(Dia::Core::StringCRC("G")));
    EXPECT_FALSE(f.parent.HasInChain(Dia::Core::StringCRC("L")));
}

TEST(BlackboardScopes, FindOwner_ReturnsCorrectBoard)
{
    ChainFixture f;
    f.child.Register<ScopeSlot>(Dia::Core::StringCRC("L"));
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("P"));
    f.grand.Register<ScopeSlot>(Dia::Core::StringCRC("G"));
    EXPECT_EQ(f.child.FindOwner(Dia::Core::StringCRC("L")), &f.child);
    EXPECT_EQ(f.child.FindOwner(Dia::Core::StringCRC("P")), &f.parent);
    EXPECT_EQ(f.child.FindOwner(Dia::Core::StringCRC("G")), &f.grand);
}

TEST(BlackboardScopes, Lookup_ParentOnlyKey_AbsentViaGetHasTryGet)
{
    ChainFixture f;
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("P"));
    EXPECT_FALSE(f.child.Has(Dia::Core::StringCRC("P")));
    EXPECT_EQ(f.child.TryGet<ScopeSlot>(Dia::Core::StringCRC("P")), nullptr);
    EXPECT_NE(f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("P")), nullptr);
}

#ifdef _DEBUG

TEST(BlackboardScopes, Lookup_TypeMismatch_Asserts)
{
    ChainFixture f;
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("K"));
    EXPECT_DEATH(f.child.Lookup<float>(Dia::Core::StringCRC("K")), "");
}

TEST(BlackboardScopes, Lookup_TypeMatchesParentButNotLocalShadow_Asserts)
{
    ChainFixture f;
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("K"));
    f.child.Register<float>(Dia::Core::StringCRC("K"));
    EXPECT_DEATH(f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K")), "");
}

#endif // _DEBUG

// -------------------------------------------------------------------------
// Edge tests (gap audit, task 10)
// -------------------------------------------------------------------------

TEST(BlackboardScopes, OneCap_HoldsExactlyOne)
{
    Blackboard board(1);
    board.Register<ScopeSlot>(Dia::Core::StringCRC("Only"));
    EXPECT_TRUE(board.Has(Dia::Core::StringCRC("Only")));
}

TEST(BlackboardScopes, Lookup_AfterParentUnregister_ReturnsNull)
{
    ChainFixture f;
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("K"));
    ASSERT_NE(f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K")), nullptr);
    f.parent.Unregister(Dia::Core::StringCRC("K"));
    EXPECT_EQ(f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K")), nullptr);
    EXPECT_FALSE(f.child.HasInChain(Dia::Core::StringCRC("K")));
    EXPECT_EQ(f.child.FindOwner(Dia::Core::StringCRC("K")), nullptr);
}

TEST(BlackboardScopes, LocalRegisterAfterInherit_ShadowsImmediately)
{
    ChainFixture f;
    f.parent.Register<ScopeSlot>(Dia::Core::StringCRC("K")).mValue = 2;
    ASSERT_EQ(f.child.FindOwner(Dia::Core::StringCRC("K")), &f.parent);
    f.child.Register<ScopeSlot>(Dia::Core::StringCRC("K")).mValue = 1;
    EXPECT_EQ(f.child.FindOwner(Dia::Core::StringCRC("K")), &f.child);
    EXPECT_EQ(f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K"))->mValue, 1);
    f.child.Unregister(Dia::Core::StringCRC("K"));
    EXPECT_EQ(f.child.Lookup<ScopeSlot>(Dia::Core::StringCRC("K"))->mValue, 2);
}

TEST(BlackboardScopes, SetParent_ParentAtDepth7_Accepted)
{
    Blackboard* boards[9];
    for (int i = 0; i < 9; ++i) boards[i] = new Blackboard();
    for (int i = 1; i < 7; ++i) ASSERT_TRUE(boards[i]->SetParent(boards[i - 1]));
    EXPECT_TRUE(boards[7]->SetParent(boards[6]));
    EXPECT_FALSE(boards[8]->SetParent(boards[7]));
    for (int i = 8; i >= 0; --i) delete boards[i];
}

TEST(BlackboardScopes, SetParent_ReparentWithOwnChildren_ChainStillValid)
{
    Blackboard a, b, mid, leaf;
    ASSERT_TRUE(leaf.SetParent(&mid));
    ASSERT_TRUE(mid.SetParent(&a));
    ASSERT_TRUE(mid.SetParent(&b));
    EXPECT_EQ(mid.GetParent(), &b);
    EXPECT_EQ(leaf.GetParent(), &mid);
    b.Register<ScopeSlot>(Dia::Core::StringCRC("K"));
    EXPECT_EQ(leaf.FindOwner(Dia::Core::StringCRC("K")), &b);
    leaf.SetParent(nullptr);
    mid.SetParent(nullptr);
}

TEST(BlackboardScopes, SetParent_ReparentUnderOwnDescendant_Refused)
{
    Blackboard a, b;
    ASSERT_TRUE(b.SetParent(&a));
    EXPECT_FALSE(a.SetParent(&b));
    EXPECT_EQ(b.GetParent(), &a);
    EXPECT_EQ(a.GetParent(), nullptr);
    b.SetParent(nullptr);
}

TEST(BlackboardScopes, TwoChildren_OneParent_BothSeeParentAndDetachIndependently)
{
    Blackboard parent;
    Blackboard c1, c2;
    parent.Register<ScopeSlot>(Dia::Core::StringCRC("K"));
    ASSERT_TRUE(c1.SetParent(&parent));
    ASSERT_TRUE(c2.SetParent(&parent));
    EXPECT_EQ(c1.FindOwner(Dia::Core::StringCRC("K")), &parent);
    EXPECT_EQ(c2.FindOwner(Dia::Core::StringCRC("K")), &parent);
    c1.SetParent(nullptr);
    EXPECT_EQ(c2.FindOwner(Dia::Core::StringCRC("K")), &parent);
    c2.SetParent(nullptr);
}

TEST(BlackboardScopes, ParentCounter_AfterChildrenDestroyed_ParentDestroysCleanly)
{
    Blackboard* parent = new Blackboard();
    {
        Blackboard c1;
        Blackboard c2;
        ASSERT_TRUE(c1.SetParent(parent));
        ASSERT_TRUE(c2.SetParent(parent));
    }
    delete parent;
}

TEST(BlackboardScopes, SetParent_RefusedDepth_LeavesExistingParent)
{
    Blackboard* boards[8];
    for (int i = 0; i < 8; ++i) boards[i] = new Blackboard();
    for (int i = 1; i < 8; ++i) ASSERT_TRUE(boards[i]->SetParent(boards[i - 1]));
    Blackboard other;
    Blackboard extra;
    ASSERT_TRUE(extra.SetParent(&other));
    EXPECT_FALSE(extra.SetParent(boards[7]));
    EXPECT_EQ(extra.GetParent(), &other);
    EXPECT_TRUE(extra.SetParent(nullptr));
    for (int i = 7; i >= 0; --i) delete boards[i];
}
