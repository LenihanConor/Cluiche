////////////////////////////////////////////////////////////////////////////////
// TestEntityVisualDebuggerDrawers.cpp
// Exhaustive tests for DiaEntityVisualDebugger drawer classes.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include "TestPosComponent.h"

#include <DiaEntityVisualDebugger/EntityPositionHelper.h>
#include <DiaEntityVisualDebugger/EntityLabelsDrawer.h>
#include <DiaEntityVisualDebugger/EntityStatsDrawer.h>
#include <DiaEntityVisualDebugger/HierarchyLinesDrawer.h>
#include <DiaEntityVisualDebugger/ComponentFilterHighlightDrawer.h>
#include <DiaEntityVisualDebugger/EntityPickingDrawer.h>
#include <DiaEntityVisualDebugger/SelectionInspectorDrawer.h>

#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/ComponentPool.h>
#include <diaentitytemplate/Hierarchy/ChildBufferComponent.h>
#include <diaentitytemplate/Hierarchy/ParentComponent.h>
#include <diaentitytemplate/Hierarchy/Hierarchy.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Entity;
using namespace Dia::EntityVisualDebugger;
using namespace Dia::Graphics;

static const Dia::Core::StringCRC kPosTypeId = DiaEntityVisualDebuggerTest::TestPosComponent::kTypeId;

// ============================================================================
// Helpers
// ============================================================================

static void RegisterPosPool(Domain& domain)
{
    domain.RegisterPool(new ComponentPool<DiaEntityVisualDebuggerTest::TestPosComponent>(kPosTypeId));
}

static void RegisterHierarchyPools(Domain& domain)
{
    domain.RegisterPool(new ComponentPool<Hierarchy::ParentComponent>(Hierarchy::ParentComponent::kTypeId));
    domain.RegisterPool(new ComponentPool<Hierarchy::ChildBufferComponent>(Hierarchy::ChildBufferComponent::kTypeId));
}

static Entity MakeEntityAtPos(Domain& domain, float x, float y, const char* name = nullptr)
{
    Entity e = name ? domain.CreateEntity(name) : domain.CreateEntity();
    Json::Value cfg;
    cfg["x"] = x;
    cfg["y"] = y;
    domain.QueueAddComponentByTypeId(e, kPosTypeId, cfg);
    domain.EndOfFrame();
    return e;
}

// ============================================================================
// EntityPositionHelper
// ============================================================================

TEST(EntityPositionHelper, ValidComponent_ReturnsXY)
{
    Domain domain;
    RegisterPosPool(domain);
    Entity e = MakeEntityAtPos(domain, 10.f, 20.f);

    Dia::Maths::Vector2D pos = EntityPositionHelper::GetPosition(domain, e, kPosTypeId);
    EXPECT_FLOAT_EQ(pos.x, 10.f);
    EXPECT_FLOAT_EQ(pos.y, 20.f);
}

TEST(EntityPositionHelper, UnknownTypeId_ReturnsZero)
{
    Domain domain;
    RegisterPosPool(domain);
    Entity e = MakeEntityAtPos(domain, 5.f, 7.f);

    Dia::Core::StringCRC unknownId("no-such-component");
    Dia::Maths::Vector2D pos = EntityPositionHelper::GetPosition(domain, e, unknownId);
    EXPECT_FLOAT_EQ(pos.x, 0.f);
    EXPECT_FLOAT_EQ(pos.y, 0.f);
}

TEST(EntityPositionHelper, InvalidEntity_ReturnsZero)
{
    Domain domain;
    RegisterPosPool(domain);
    // Don't create any entity — use Invalid()
    Dia::Maths::Vector2D pos = EntityPositionHelper::GetPosition(domain, Entity::Invalid(), kPosTypeId);
    EXPECT_FLOAT_EQ(pos.x, 0.f);
    EXPECT_FLOAT_EQ(pos.y, 0.f);
}

// ============================================================================
// GlobMatch
// ============================================================================

TEST(EntityPositionHelper_GlobMatch, StarMatchesAll)
{
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("*", "anything"));
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("*", ""));
}

TEST(EntityPositionHelper_GlobMatch, PrefixStar)
{
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("child*", "childA"));
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("child*", "childFoo"));
    EXPECT_FALSE(EntityPositionHelper::GlobMatch("child*", "parent"));
}

TEST(EntityPositionHelper_GlobMatch, QuestionMark_SingleChar)
{
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("e?t", "eat"));
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("e?t", "eet"));
    EXPECT_FALSE(EntityPositionHelper::GlobMatch("e?t", "et"));
    EXPECT_FALSE(EntityPositionHelper::GlobMatch("e?t", "eaat"));
}

TEST(EntityPositionHelper_GlobMatch, ExactMatch)
{
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("hello", "hello"));
    EXPECT_FALSE(EntityPositionHelper::GlobMatch("hello", "world"));
}

TEST(EntityPositionHelper_GlobMatch, EmptyPatternEmptyText)
{
    EXPECT_TRUE(EntityPositionHelper::GlobMatch("", ""));
    EXPECT_FALSE(EntityPositionHelper::GlobMatch("", "x"));
}

// ============================================================================
// EntityLabelsDrawer
// ============================================================================

TEST(EntityLabelsDrawer, NoEntities_ZeroTextPrimitives)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityLabelsDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetTextPrimitiveCount(), 0u);
}

TEST(EntityLabelsDrawer, DefaultStarFilter_LabelsAllNamedEntities)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f, "Alpha");
    MakeEntityAtPos(domain, 1.f, 0.f, "Beta");

    Dia::Debug::DebugLayerManager mgr;
    EntityLabelsDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetTextPrimitiveCount(), 2u);
}

TEST(EntityLabelsDrawer, FilterReducesLabels)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f, "child_A");
    MakeEntityAtPos(domain, 1.f, 0.f, "child_B");
    MakeEntityAtPos(domain, 2.f, 0.f, "parent");

    Dia::Debug::DebugLayerManager mgr;
    EntityLabelsDrawer drawer(domain, domain, mgr, kPosTypeId);
    drawer.SetFilter("child*");

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetTextPrimitiveCount(), 2u);
}

TEST(EntityLabelsDrawer, NoMatchFilter_ZeroLabels)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f, "Alpha");

    Dia::Debug::DebugLayerManager mgr;
    EntityLabelsDrawer drawer(domain, domain, mgr, kPosTypeId);
    drawer.SetFilter("xyz*");

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetTextPrimitiveCount(), 0u);
}

TEST(EntityLabelsDrawer, EntityWithNoDebugName_NotDrawn)
{
    Domain domain;
    RegisterPosPool(domain);
    // CreateEntity without name — debug name is null
    Entity e = domain.CreateEntity();
    Json::Value cfg; cfg["x"] = 0.f; cfg["y"] = 0.f;
    domain.QueueAddComponentByTypeId(e, kPosTypeId, cfg);
    domain.EndOfFrame();

    Dia::Debug::DebugLayerManager mgr;
    EntityLabelsDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetTextPrimitiveCount(), 0u);
}

// ============================================================================
// EntityStatsDrawer
// ============================================================================

TEST(EntityStatsDrawer, Draw_NoPrimitives)
{
    Domain domain;
    Dia::Debug::DebugLayerManager mgr;
    EntityStatsDrawer drawer(domain);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 0u);
    EXPECT_EQ(frame.GetTextPrimitiveCount(), 0u);
}

TEST(EntityStatsDrawer, GetLayerName_Correct)
{
    Domain domain;
    EntityStatsDrawer drawer(domain);
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kEntityStats);
}

// ============================================================================
// HierarchyLinesDrawer
// ============================================================================

TEST(HierarchyLinesDrawer, NoHierarchy_ZeroLines)
{
    Domain domain;
    RegisterPosPool(domain);
    RegisterHierarchyPools(domain);
    MakeEntityAtPos(domain, 0.f, 0.f, "orphan");

    Dia::Debug::DebugLayerManager mgr;
    HierarchyLinesDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 0u);
}

TEST(HierarchyLinesDrawer, ParentWithThreeChildren_ThreeLines)
{
    Domain domain;
    RegisterPosPool(domain);
    RegisterHierarchyPools(domain);

    Entity parent = MakeEntityAtPos(domain, 0.f, 0.f, "parent");
    Entity childA = MakeEntityAtPos(domain, 10.f, 0.f, "childA");
    Entity childB = MakeEntityAtPos(domain, 20.f, 0.f, "childB");
    Entity childC = MakeEntityAtPos(domain, 30.f, 0.f, "childC");

    Hierarchy::QueueSetParent(domain, childA, parent);
    Hierarchy::QueueSetParent(domain, childB, parent);
    Hierarchy::QueueSetParent(domain, childC, parent);
    domain.EndOfFrame();

    Dia::Debug::DebugLayerManager mgr;
    HierarchyLinesDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 3u);
}

TEST(HierarchyLinesDrawer, DeepHierarchy_DrawsEachParentChildPair)
{
    Domain domain;
    RegisterPosPool(domain);
    RegisterHierarchyPools(domain);

    Entity grandparent = MakeEntityAtPos(domain, 0.f, 0.f, "grandparent");
    Entity parent      = MakeEntityAtPos(domain, 10.f, 0.f, "parent");
    Entity child       = MakeEntityAtPos(domain, 20.f, 0.f, "child");

    Hierarchy::QueueSetParent(domain, parent, grandparent);
    Hierarchy::QueueSetParent(domain, child,  parent);
    domain.EndOfFrame();

    Dia::Debug::DebugLayerManager mgr;
    HierarchyLinesDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    // grandparent→parent line + parent→child line = 2
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 2u);
}

// ============================================================================
// ComponentFilterHighlightDrawer
// ============================================================================

TEST(ComponentFilterHighlightDrawer, NoFilter_ZeroPrimitives)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f);

    Dia::Debug::DebugLayerManager mgr;
    ComponentFilterHighlightDrawer drawer(domain, domain, mgr, kPosTypeId);
    // Filter is zero by default

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 0u);
}

TEST(ComponentFilterHighlightDrawer, FilterMatchesTwoEntities_TwoCircles)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f);
    MakeEntityAtPos(domain, 5.f, 0.f);

    Dia::Debug::DebugLayerManager mgr;
    ComponentFilterHighlightDrawer drawer(domain, domain, mgr, kPosTypeId);
    drawer.SetFilterTypeId(kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 2u);
}

TEST(ComponentFilterHighlightDrawer, UnknownTypeId_ZeroPrimitives)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f);

    Dia::Debug::DebugLayerManager mgr;
    ComponentFilterHighlightDrawer drawer(domain, domain, mgr, kPosTypeId);
    drawer.SetFilterTypeId(Dia::Core::StringCRC("no-such-type"));

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 0u);
}

// ============================================================================
// EntityPickingDrawer
// ============================================================================

TEST(EntityPickingDrawer, NoSelection_ZeroPrimitives)
{
    Domain domain;
    RegisterPosPool(domain);

    Dia::Debug::DebugLayerManager mgr;
    // mSelectedEntityId defaults to 0
    EntityPickingDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 0u);
}

TEST(EntityPickingDrawer, ValidSelection_DrawsHighlightCircle)
{
    Domain domain;
    RegisterPosPool(domain);
    Entity e = MakeEntityAtPos(domain, 0.f, 0.f);

    Dia::Debug::DebugLayerManager mgr;
    mgr.SetSelectedEntityId(e.GetIndex() + 1);

    EntityPickingDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_GE(frame.GetDebugPrimitiveCount(), 1u);
}

TEST(EntityPickingDrawer, DeadEntity_NoCrashZeroPrimitives)
{
    Domain domain;
    RegisterPosPool(domain);
    Entity e = MakeEntityAtPos(domain, 0.f, 0.f);
    uint32_t entityIdx = e.GetIndex();
    domain.QueueDestroy(e);
    domain.EndOfFrame();

    Dia::Debug::DebugLayerManager mgr;
    mgr.SetSelectedEntityId(entityIdx + 1);

    EntityPickingDrawer drawer(domain, domain, mgr, kPosTypeId);

    FrameData frame;
    EXPECT_NO_FATAL_FAILURE(drawer.Draw(frame));
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 0u);
}

// ============================================================================
// SelectionInspectorDrawer
// ============================================================================

TEST(SelectionInspectorDrawer, Draw_NoPrimitives)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    SelectionInspectorDrawer drawer(domain, domain, mgr);

    FrameData frame;
    drawer.Draw(frame);
    EXPECT_EQ(frame.GetDebugPrimitiveCount(), 0u);
    EXPECT_EQ(frame.GetTextPrimitiveCount(), 0u);
}

TEST(SelectionInspectorDrawer, GetLayerName_Correct)
{
    Domain domain;
    Dia::Debug::DebugLayerManager mgr;
    SelectionInspectorDrawer drawer(domain, domain, mgr);
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kEntityInspector);
}
