////////////////////////////////////////////////////////////////////////////////
// Filename: TestInputRouter.cpp - Google Test for InputRouter
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/InputRouter.h>
#include <thread>

using namespace Dia::Input;

////////////////////////////////////////////////////////////////////////////////
// Empty stack behaviour

TEST(InputRouter, EmptyStackReturnsGameOnly)
{
    InputRouter r;
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
}

TEST(InputRouter, PopOnEmptyIsNoOp)
{
    InputRouter r;
    r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
}

////////////////////////////////////////////////////////////////////////////////
// Single-element push / pop

TEST(InputRouter, PushUIOnlyReturnsUIOnly)
{
    InputRouter r;
    r.PushInputMode(EInputRouting::kUIOnly);
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kUIOnly);
}

TEST(InputRouter, PushGameAndUIReturnsGameAndUI)
{
    InputRouter r;
    r.PushInputMode(EInputRouting::kGameAndUI);
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameAndUI);
}

TEST(InputRouter, PopAfterSinglePushReturnsGameOnly)
{
    InputRouter r;
    r.PushInputMode(EInputRouting::kUIOnly);
    r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
}

////////////////////////////////////////////////////////////////////////////////
// Stack ordering (LIFO)

TEST(InputRouter, StackIsLIFO)
{
    InputRouter r;
    r.PushInputMode(EInputRouting::kGameOnly);
    r.PushInputMode(EInputRouting::kUIOnly);
    r.PushInputMode(EInputRouting::kGameAndUI);

    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameAndUI);
    r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kUIOnly);
    r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
    r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly); // empty → default
}

TEST(InputRouter, PopBeyondPushedCountReturnsGameOnly)
{
    InputRouter r;
    r.PushInputMode(EInputRouting::kUIOnly);
    r.PopInputMode();
    r.PopInputMode(); // extra pop — must not corrupt state
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
}

////////////////////////////////////////////////////////////////////////////////
// Stack capacity (DynamicArrayC capacity = 8)

TEST(InputRouter, PushAtCapacityDropsExtraItems)
{
    InputRouter r;
    // Fill to capacity
    for (int i = 0; i < 8; i++)
        r.PushInputMode(EInputRouting::kUIOnly);
    // 9th push is silently dropped (IsFull guard)
    r.PushInputMode(EInputRouting::kGameAndUI);
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kUIOnly);

    // Exactly 8 pops drain the stack
    for (int i = 0; i < 8; i++)
        r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
}

TEST(InputRouter, FullThenPopAllRestoresEmpty)
{
    InputRouter r;
    for (int i = 0; i < 8; i++)
        r.PushInputMode(EInputRouting::kGameAndUI);
    for (int i = 0; i < 8; i++)
        r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
}

////////////////////////////////////////////////////////////////////////////////
// Thread safety

TEST(InputRouter, ConcurrentPushPopAndReadNoRace)
{
    InputRouter r;
    r.PushInputMode(EInputRouting::kUIOnly);

    // Writer thread: push + pop alternately 2000 times
    std::thread writer([&r]() {
        for (int i = 0; i < 2000; i++)
        {
            r.PushInputMode(EInputRouting::kGameAndUI);
            r.PopInputMode();
        }
    });

    // Reader thread: reads 2000 times — must not deadlock or crash
    std::thread reader([&r]() {
        for (int i = 0; i < 2000; i++)
            (void)r.GetCurrentInputMode();
    });

    writer.join();
    reader.join();
    // Stack should be back to just kUIOnly after writer's paired push/pops
    r.PopInputMode();
    EXPECT_EQ(r.GetCurrentInputMode(), EInputRouting::kGameOnly);
}
