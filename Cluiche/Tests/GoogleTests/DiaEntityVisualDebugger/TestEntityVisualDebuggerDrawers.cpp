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
#include <DiaEntityVisualDebugger/EntityDebugDomain.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Hierarchy/Hierarchy.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstring>

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

// ============================================================================
// EntityDebugDomain — AC-15 mandatory IDebugDomain test shapes
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
// ============================================================================

namespace
{

Json::Value MakeToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

// Radius of the first Circle2D primitive in the frame, or -1 if absent.
float FirstCircleRadius(const FrameData& fd)
{
    Dia::Graphics::Testing::InspectingDebugVisitor v;
    static_cast<const Dia::Graphics::DebugFrameData&>(fd).AcceptVisitor(v);
    if (v.visitCount == 0) return -1.0f;
    if (v.lastPrimitive.type != DebugPrimitiveType::Circle2D) return -1.0f;
    return v.lastPrimitive.circle2D.radius;
}

} // namespace

TEST(EntityDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    Domain domain;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);

    EXPECT_EQ(dbg.GetDomainId(), Dia::Core::StringCRC("Entity"));
    EXPECT_STREQ(dbg.GetDisplayName(), "Entity");
    EXPECT_EQ(dbg.GetGroup(), Dia::Core::StringCRC("Entity"));
    EXPECT_TRUE(dbg.HasWorldDrawers());
}

TEST(EntityDebugDomain_Identity, DescriptionWithin80Chars)
{
    Domain domain;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);

    ASSERT_NE(dbg.GetDescription(), nullptr);
    EXPECT_LE(strlen(dbg.GetDescription()), 80u);
}

TEST(EntityDebugDomain_Identity, AccentIsEntityGroupConstant)
{
    Domain domain;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);

    EXPECT_EQ(dbg.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kEntity);
}

TEST(EntityDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);

    EXPECT_EQ(dbg.GetDrawerCount(), 0);
    EXPECT_EQ(dbg.GetDrawer(0), nullptr);

    dbg.Register(mgr);

    EXPECT_EQ(dbg.GetDrawerCount(), EntityDebugDomain::kDrawerCount);
    for (int i = 0; i < EntityDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(dbg.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(dbg.GetDrawer(EntityDebugDomain::kDrawerCount), nullptr);
}

TEST(EntityDebugDomain_Lifecycle, RegisterAddsAllSixLayers)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), EntityDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kEntityLabels));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kEntityHierarchy));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kEntityHighlight));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kEntityPicking));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kEntityStats));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kEntityInspector));
}

TEST(EntityDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);
    dbg.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(dbg.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kEntityLabels));
}

TEST(EntityDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);
    dbg.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), EntityDebugDomain::kDrawerCount);
}

// ---- AC-15 #1: enable/disable gate --------------------------------------

TEST(EntityDebugDomain_DrawerGate, DisabledLabelsDrawerEmitsNoText)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f, "Alpha");

    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    ASSERT_EQ(enabledFrame.GetTextPrimitiveCount(), 1u) << "precondition: label drawn when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kEntityLabels);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(disabledFrame.GetTextPrimitiveCount(), 0u);
}

TEST(EntityDebugDomain_DrawerGate, DisablingEveryLayerEmitsNothing)
{
    Domain domain;
    RegisterPosPool(domain);
    RegisterHierarchyPools(domain);
    Entity parent = MakeEntityAtPos(domain, 0.f, 0.f, "parent");
    Entity child  = MakeEntityAtPos(domain, 5.f, 0.f, "child");
    Hierarchy::QueueSetParent(domain, child, parent);
    domain.EndOfFrame();

    Dia::Debug::DebugLayerManager mgr;
    mgr.SetSelectedEntityId(parent.GetIndex() + 1);
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    for (int i = 0; i < EntityDebugDomain::kDrawerCount; ++i)
        mgr.DisableLayer(dbg.GetDrawer(i)->GetLayerName());

    FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(fd.GetDebugPrimitiveCount(), 0u);
    EXPECT_EQ(fd.GetTextPrimitiveCount(), 0u);
}

// ---- AC-15 #2: expected primitive types ---------------------------------
// Labels emit text, Hierarchy emits lines, Picking emits a circle.
// Stats and Inspector emit nothing in world space by design — they are still
// reported by GetJSONState() (see EntityDebugDomain_JSONState below).

TEST(EntityDebugDomain_Primitives, AllEnabledDrawersEmitExpectedTypes)
{
    Domain domain;
    RegisterPosPool(domain);
    RegisterHierarchyPools(domain);
    Entity parent = MakeEntityAtPos(domain, 0.f, 0.f, "parent");
    Entity child  = MakeEntityAtPos(domain, 5.f, 0.f, "child");
    Hierarchy::QueueSetParent(domain, child, parent);
    domain.EndOfFrame();

    Dia::Debug::DebugLayerManager mgr;
    mgr.SetSelectedEntityId(parent.GetIndex() + 1);
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    FrameData fd;
    mgr.Draw(fd);

    Dia::Graphics::Testing::RecordingDebugVisitor v;
    static_cast<const Dia::Graphics::DebugFrameData&>(fd).AcceptVisitor(v);

    EXPECT_EQ(v.TextCount(),   2) << "labels: one per named entity";
    EXPECT_EQ(v.LineCount(),   1) << "hierarchy: one parent-child line";
    EXPECT_EQ(v.CircleCount(), 1) << "picking: one selection ring";
}

// ---- AC-15 #3: scale sensitivity ----------------------------------------

TEST(EntityDebugDomain_Scale, DoublingDebugScaleDoublesSelectionRingRadius)
{
    Domain domain;
    RegisterPosPool(domain);
    Entity e = MakeEntityAtPos(domain, 0.f, 0.f);   // unnamed → no label text

    Dia::Debug::DebugLayerManager mgr;
    mgr.SetSelectedEntityId(e.GetIndex() + 1);
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    mgr.SetDebugScale(1.0f);
    FrameData frame1;
    mgr.Draw(frame1);
    const float radius1 = FirstCircleRadius(frame1);
    ASSERT_GT(radius1, 0.0f);

    mgr.SetDebugScale(2.0f);
    FrameData frame2;
    mgr.Draw(frame2);
    const float radius2 = FirstCircleRadius(frame2);

    EXPECT_NEAR(radius2, radius1 * 2.0f, 1e-4f);
}

TEST(EntityDebugDomain_Scale, DoublingDebugScaleDoublesLabelFontSize)
{
    Domain domain;
    RegisterPosPool(domain);
    MakeEntityAtPos(domain, 0.f, 0.f, "Alpha");

    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    Dia::Graphics::Testing::InspectingDebugVisitor v1;
    mgr.SetDebugScale(1.0f);
    FrameData frame1;
    mgr.Draw(frame1);
    static_cast<const Dia::Graphics::DebugFrameData&>(frame1).AcceptVisitor(v1);
    ASSERT_EQ(v1.textVisitCount, 1);

    Dia::Graphics::Testing::InspectingDebugVisitor v2;
    mgr.SetDebugScale(2.0f);
    FrameData frame2;
    mgr.Draw(frame2);
    static_cast<const Dia::Graphics::DebugFrameData&>(frame2).AcceptVisitor(v2);
    ASSERT_EQ(v2.textVisitCount, 1);

    EXPECT_NEAR(v2.lastText.fontSize, v1.lastText.fontSize * 2.0f, 1e-4f);
}

// ---- AC-15 #4: GetJSONState round-trip ----------------------------------

TEST(EntityDebugDomain_JSONState, ReportsEveryDrawerAndAStatsObject)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    Json::Value state;
    dbg.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(EntityDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        const Json::Value& entry = state["drawers"][i];
        EXPECT_TRUE(entry["name"].isString());
        EXPECT_FALSE(entry["name"].asString().empty());
        EXPECT_TRUE(entry["enabled"].asBool()) << "drawers start enabled";
    }
}

TEST(EntityDebugDomain_JSONState, PanelOnlyDrawersAreStillReported)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    Json::Value state;
    dbg.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 6u);
    // Stats (index 4) and Inspector (index 5) draw nothing in world space but
    // must still appear in the panel so they can be toggled.
    EXPECT_STREQ(state["drawers"][4u]["name"].asCString(), "Stats");
    EXPECT_STREQ(state["drawers"][5u]["name"].asCString(), "Inspector");
}

TEST(EntityDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kEntityLabels);

    Json::Value state;
    dbg.GetJSONState(state);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Labels");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(EntityDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    Domain domain;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);

    Json::Value state;
    dbg.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 6u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ---- AC-15 #5: OnCommand("toggle") round-trip ---------------------------

TEST(EntityDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    const Dia::Core::StringCRC labels = Dia::Debug::LayerNames::kEntityLabels;
    ASSERT_TRUE(mgr.IsLayerEnabled(labels));

    dbg.OnCommand(Dia::Core::StringCRC("toggle"), MakeToggleArgs("Labels"));
    EXPECT_FALSE(mgr.IsLayerEnabled(labels));

    dbg.OnCommand(Dia::Core::StringCRC("toggle"), MakeToggleArgs("Labels"));
    EXPECT_TRUE(mgr.IsLayerEnabled(labels));
}

TEST(EntityDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    dbg.OnCommand(Dia::Core::StringCRC("toggle"), MakeToggleArgs("entity.picking"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kEntityPicking));
}

TEST(EntityDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    dbg.OnCommand(Dia::Core::StringCRC("toggle"), MakeToggleArgs("Stats"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kEntityStats));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kEntityLabels));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kEntityHierarchy));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kEntityHighlight));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kEntityPicking));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kEntityInspector));
}

TEST(EntityDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    dbg.OnCommand(Dia::Core::StringCRC("toggle"), MakeToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < EntityDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(dbg.GetDrawer(i)->GetLayerName()));
}

TEST(EntityDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    Domain domain;
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);

    dbg.OnCommand(Dia::Core::StringCRC("toggle"), MakeToggleArgs("Labels"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(EntityDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 3.0;
    dbg.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 3.0f);
}

// ---- Stats fields: GetJSONState() entity count assertions ----------------

TEST(EntityDebugDomain_JSONState_Stats, EntityCountFieldsPresent)
{
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    Json::Value state;
    dbg.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("alive"))       << "stats.alive missing";
    EXPECT_TRUE(stats["alive"].isInt());
    EXPECT_TRUE(stats.isMember("queryCount"))  << "stats.queryCount missing";
    EXPECT_TRUE(stats["queryCount"].isInt());
    EXPECT_TRUE(stats.isMember("selection"))   << "stats.selection missing";
    EXPECT_TRUE(stats["selection"].isObject());
    EXPECT_TRUE(stats["selection"].isMember("id"))           << "stats.selection.id missing";
    EXPECT_TRUE(stats["selection"].isMember("hasSelection")) << "stats.selection.hasSelection missing";
}

TEST(EntityDebugDomain_JSONState_Stats, AliveCountIsZeroWithEmptyDomain)
{
    // Empty domain (no entities created) — alive == 0, no selection active.
    Domain domain;
    RegisterPosPool(domain);
    Dia::Debug::DebugLayerManager mgr;
    EntityDebugDomain dbg(domain, domain, kPosTypeId);
    dbg.Register(mgr);

    Json::Value state;
    dbg.GetJSONState(state);

    EXPECT_EQ(state["stats"]["alive"].asInt(), 0);
    EXPECT_FALSE(state["stats"]["selection"]["hasSelection"].asBool());
}
