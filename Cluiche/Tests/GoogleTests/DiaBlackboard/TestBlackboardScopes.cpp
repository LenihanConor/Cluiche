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
