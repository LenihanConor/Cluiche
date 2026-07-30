#include <gtest/gtest.h>

#include <DiaUtilityAI/GroupConsiderationContext.h>
#include <DiaUtilityAI/ScorerDef.h>
#include <DiaUtilityAI/ActionDef.h>
#include <DiaCore/CRC/StringCRC.h>

// ---------------------------------------------------------------------------
// DiaUtilityAI_GroupConsiderationContext
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_GroupConsiderationContext, GetCount_NoEntries_ReturnsZero)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC anyAction("attack");
    EXPECT_EQ(ctx.GetCount(anyAction), 0);
}

TEST(DiaUtilityAI_GroupConsiderationContext, Increment_Once_ReturnsOne)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC action("attack");
    ctx.Increment(action);
    EXPECT_EQ(ctx.GetCount(action), 1);
}

TEST(DiaUtilityAI_GroupConsiderationContext, Increment_Twice_ReturnsTwo)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC action("attack");
    ctx.Increment(action);
    ctx.Increment(action);
    EXPECT_EQ(ctx.GetCount(action), 2);
}

TEST(DiaUtilityAI_GroupConsiderationContext, Decrement_AfterIncrement_ReturnsZero)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC action("attack");
    ctx.Increment(action);
    ctx.Decrement(action);
    EXPECT_EQ(ctx.GetCount(action), 0);
}

TEST(DiaUtilityAI_GroupConsiderationContext, Decrement_BelowZero_StaysAtZero)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC action("attack");
    ctx.Decrement(action);
    EXPECT_EQ(ctx.GetCount(action), 0);
}

TEST(DiaUtilityAI_GroupConsiderationContext, GetCount_UnregisteredAction_ReturnsZero)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC registered("attack");
    Dia::Core::StringCRC unregistered("flee");
    ctx.Increment(registered);
    EXPECT_EQ(ctx.GetCount(unregistered), 0);
}

TEST(DiaUtilityAI_GroupConsiderationContext, Reset_ClearsAllCounts)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC actionA("attack");
    Dia::Core::StringCRC actionB("defend");
    ctx.Increment(actionA);
    ctx.Increment(actionB);
    ctx.Reset();
    EXPECT_EQ(ctx.GetCount(actionA), 0);
    EXPECT_EQ(ctx.GetCount(actionB), 0);
}

TEST(DiaUtilityAI_GroupConsiderationContext, Increment_DifferentActions_TrackIndependently)
{
    Dia::UtilityAI::GroupConsiderationContext ctx;
    Dia::Core::StringCRC actionA("attack");
    Dia::Core::StringCRC actionB("defend");
    ctx.Increment(actionA);
    ctx.Increment(actionA);
    ctx.Increment(actionB);
    EXPECT_EQ(ctx.GetCount(actionA), 2);
    EXPECT_EQ(ctx.GetCount(actionB), 1);
}

// ---------------------------------------------------------------------------
// DiaUtilityAI_ScorerDef
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ScorerDef, DefaultConstruct_HasZeroMinMax)
{
    Dia::UtilityAI::ScorerDef s;
    EXPECT_FLOAT_EQ(s.inputMin, 0.0f);
    EXPECT_FLOAT_EQ(s.inputMax, 1.0f);
}

// ---------------------------------------------------------------------------
// DiaUtilityAI_ActionDef
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ActionDef, DefaultConstruct_ZeroCooldown)
{
    Dia::UtilityAI::ActionDef a;
    EXPECT_FLOAT_EQ(a.cooldownSeconds, 0.0f);
}

TEST(DiaUtilityAI_ActionDef, DefaultConstruct_ZeroMaxConcurrent)
{
    Dia::UtilityAI::ActionDef a;
    EXPECT_EQ(a.maxConcurrent, 0);
}

TEST(DiaUtilityAI_ActionDef, MoveConstruct_Works)
{
    Dia::UtilityAI::ActionDef a;
    Dia::UtilityAI::ActionDef b(std::move(a));
    // No crash = pass
    EXPECT_FLOAT_EQ(b.cooldownSeconds, 0.0f);
}
