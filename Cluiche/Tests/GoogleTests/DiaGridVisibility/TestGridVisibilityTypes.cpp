// TestGridVisibilityTypes.cpp — GoogleTest coverage for DiaGridVisibility type foundations.
//
// Suites: GridVisibilityGroupIdType, GridVisibilityStateType

#include <gtest/gtest.h>

#include "DiaGridVisibility/VisibilityGroupId.h"
#include "DiaGridVisibility/VisibilityState.h"

using Dia::GridVisibility::VisibilityGroupId;
using Dia::GridVisibility::VisibilityState;

// ---------------------------------------------------------------------------
// VisibilityGroupId — StringCRC alias
// ---------------------------------------------------------------------------
TEST(GridVisibilityGroupIdType, SameStringProducesSameId)
{
    const VisibilityGroupId a("Blue");
    const VisibilityGroupId b("Blue");
    EXPECT_EQ(a, b);
}

TEST(GridVisibilityGroupIdType, DifferentStringsProduceDifferentIds)
{
    const VisibilityGroupId blue("Blue");
    const VisibilityGroupId red("Red");
    EXPECT_NE(blue, red);
}

TEST(GridVisibilityGroupIdType, DefaultConstructedIdsAreEqual)
{
    const VisibilityGroupId a;
    const VisibilityGroupId b;
    EXPECT_EQ(a, b);
}

TEST(GridVisibilityGroupIdType, DefaultConstructedDiffersFromNamedId)
{
    const VisibilityGroupId unnamed;
    const VisibilityGroupId named("Player");
    EXPECT_NE(unnamed, named);
}

// ---------------------------------------------------------------------------
// VisibilityState — tri-state enum
// ---------------------------------------------------------------------------
TEST(GridVisibilityStateType, UnexploredIsZero)
{
    EXPECT_EQ(static_cast<uint8_t>(VisibilityState::Unexplored), 0u);
}

TEST(GridVisibilityStateType, RevealedIsOne)
{
    EXPECT_EQ(static_cast<uint8_t>(VisibilityState::Revealed), 1u);
}

TEST(GridVisibilityStateType, VisibleIsTwo)
{
    EXPECT_EQ(static_cast<uint8_t>(VisibilityState::Visible), 2u);
}

TEST(GridVisibilityStateType, OrderingIsUnexploredRevealedVisible)
{
    // Numerical order encodes the progression from unknown → seen → active.
    EXPECT_LT(static_cast<uint8_t>(VisibilityState::Unexplored),
              static_cast<uint8_t>(VisibilityState::Revealed));
    EXPECT_LT(static_cast<uint8_t>(VisibilityState::Revealed),
              static_cast<uint8_t>(VisibilityState::Visible));
}
