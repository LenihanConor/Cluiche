// TestGridVisibilityDebugDomain.cpp
// Unit tests for GridVisibilityDebugDomain<MockVisibilityGraph>.
// Covers ACs D-1 through D-8 of DiaGridVisibilityVisualDebugger spec.
//
// Suites:
//   GridVisibilityDebugDomain_Identity
//   GridVisibilityDebugDomain_Lifecycle
//   GridVisibilityDebugDomain_DrawerGate
//   GridVisibilityDebugDomain_JSONState
//   GridVisibilityDebugDomain_OnCommand

#include <gtest/gtest.h>
#ifdef DIA_DEBUG

#include <DiaGridVisibilityVisualDebugger/GridVisibilityDebugDomain.h>
#include <DiaGridVisibility/Testing/VisibilityTestHelpers.h>
#include <DiaGridVisibility/GridVisibilitySystem.h>
#include <DiaGridVisibility/VisibilityGroupId.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <memory>
#include <cstring>

using MockGraph = Dia::GridVisibility::Testing::MockVisibilityGraph;
using GVSystem  = Dia::GridVisibility::GridVisibilitySystem<MockGraph>;
using GVDomain  = Dia::GridVisibilityVisualDebugger::GridVisibilityDebugDomain<MockGraph>;
using MGR       = Dia::Debug::DebugLayerManager;

namespace
{

// ---------------------------------------------------------------------------
// MockDebugDraw
// Counts draw call types to verify drawer output.
// ---------------------------------------------------------------------------
struct MockDebugDraw : public Dia::Core::IDebugDraw
{
    int mRectCount   = 0;
    int mCircleCount = 0;
    int mLineCount   = 0;

    // Circle2D — 4-arg (used by both 3-arg and 4-arg overloads)
    void RequestDraw(const Dia::Maths::Vector2D&, float,
                     Dia::Core::RGBA, Dia::Core::RGBA) override
    { ++mCircleCount; }

    // Line2D
    void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                     Dia::Core::RGBA) override
    { ++mLineCount; }

    // Rect2D
    void RequestDrawRect(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                         Dia::Core::RGBA, Dia::Core::RGBA) override
    { ++mRectCount; }

    void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
    void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float, Dia::Core::RGBA) override {}
    void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                        float, Dia::Core::RGBA) override {}

    // Triangle2D
    void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                     const Dia::Maths::Vector2D&,
                     Dia::Core::RGBA, Dia::Core::RGBA) override {}

    void RequestDrawText(const Dia::Maths::Vector2D&, const char*, float,
                         Dia::Core::RGBA) override {}
    void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                           Dia::Core::RGBA) override {}
    void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                          float, Dia::Core::RGBA) override {}
    void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                          Dia::Core::RGBA) override {}
    void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float,
                             Dia::Core::RGBA) override {}
    void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                            float, float, Dia::Core::RGBA) override {}

    uint32_t DroppedCount() const override { return 0; }
    const Dia::Maths::Vector2D& GetMousePixel() const override
    {
        static Dia::Maths::Vector2D z;
        return z;
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Dia::EntitySpatial::EntitySpatialIndex::SquareDef MakeSquareDef()
{
    Dia::EntitySpatial::EntitySpatialIndex::SquareDef def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(0.0f, 0.0f),
        Dia::Maths::Vector2D(100.0f, 100.0f));
    def.cellSize = 10.0f;
    return def;
}

// ---------------------------------------------------------------------------
// DomainFixture
//
// Declaration order matches C++ initialization order (construction top-down,
// destruction bottom-up). system must outlive domain because domain holds a ref.
// ---------------------------------------------------------------------------
struct DomainFixture
{
    MockGraph                               graph;
    std::unique_ptr<GVSystem>               system;
    Dia::Entity::Domain                     entityDomain;
    Dia::EntitySpatial::EntitySpatialModule spatial;
    GVDomain                                domain;

    DomainFixture()
        : graph{}
        , system(std::make_unique<GVSystem>(graph, 1))
        , entityDomain{}
        , spatial(entityDomain, MakeSquareDef())
        , domain(*system, spatial, 1.0f)
    {}
};

} // anonymous namespace

// ===========================================================================
// Suite: GridVisibilityDebugDomain_Identity
// Covers ACs D-1, D-2, D-3 and related identity checks.
// ===========================================================================

TEST(GridVisibilityDebugDomain_Identity, D1_GetDomainId)
{
    DomainFixture f;
    EXPECT_EQ(f.domain.GetDomainId(), Dia::Core::StringCRC("GridVisibility"));
}

TEST(GridVisibilityDebugDomain_Identity, D2_HasWorldDrawers)
{
    DomainFixture f;
    EXPECT_TRUE(f.domain.HasWorldDrawers());
}

TEST(GridVisibilityDebugDomain_Identity, D3_GetDrawerCount)
{
    DomainFixture f;
    EXPECT_EQ(f.domain.GetDrawerCount(), 3);
    EXPECT_NE(f.domain.GetDrawer(0), nullptr);
    EXPECT_NE(f.domain.GetDrawer(1), nullptr);
    EXPECT_NE(f.domain.GetDrawer(2), nullptr);
    EXPECT_EQ(f.domain.GetDrawer(3), nullptr);
}

TEST(GridVisibilityDebugDomain_Identity, D1_DescriptionWithin80Chars)
{
    DomainFixture f;
    EXPECT_LE(std::strlen(f.domain.GetDescription()), 80u);
}

TEST(GridVisibilityDebugDomain_Identity, AccentIsSpatialGroupConstant)
{
    DomainFixture f;
    EXPECT_EQ(f.domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kSpatial);
}

TEST(GridVisibilityDebugDomain_Identity, GroupIsSpatial)
{
    DomainFixture f;
    EXPECT_EQ(f.domain.GetGroup(), Dia::Core::StringCRC("Spatial"));
}

// ===========================================================================
// Suite: GridVisibilityDebugDomain_Lifecycle
// Covers Register / Unregister / idempotency / stage tag.
// ===========================================================================

TEST(GridVisibilityDebugDomain_Lifecycle, RegisterAddsThreeLayers)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 3);
    EXPECT_TRUE(mgr.HasLayer(Dia::Core::StringCRC("GV.CellState")));
    EXPECT_TRUE(mgr.HasLayer(Dia::Core::StringCRC("GV.SightRadii")));
    EXPECT_TRUE(mgr.HasLayer(Dia::Core::StringCRC("GV.Boundary")));
}

TEST(GridVisibilityDebugDomain_Lifecycle, LayersCarryGridVisibilityStageTag)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("GridVisibility")));
}

TEST(GridVisibilityDebugDomain_Lifecycle, UnregisterRemovesAllLayers)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);
    f.domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(GridVisibilityDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);
    f.domain.Register(mgr);  // second call is no-op

    EXPECT_EQ(mgr.GetLayerCount(), 3);
}

// ===========================================================================
// Suite: GridVisibilityDebugDomain_DrawerGate
// Covers AC D-4 (CellState emits rects) and D-6 (Boundary starts disabled).
// ===========================================================================

TEST(GridVisibilityDebugDomain_DrawerGate, D6_BoundaryDrawerStartsDisabled)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    // ShadowcastBoundaryDrawer calls SetEnabled(false) in its constructor
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.Boundary")));

    // The other two drawers start enabled
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.CellState")));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.SightRadii")));
}

TEST(GridVisibilityDebugDomain_DrawerGate, D4_CellStateDrawerEmitsRects)
{
    DomainFixture f;
    MockDebugDraw draw;

    // Call directly on the drawer — bypasses layer manager toggle.
    // 8×8 vis grid with chunkSize=1 → 64 cells → 64 rect calls.
    f.domain.GetDrawer(0)->Draw(draw);

    EXPECT_EQ(draw.mRectCount, 64);
}

TEST(GridVisibilityDebugDomain_DrawerGate, DisabledCellStateDrawerEmitsNothing)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    mgr.DisableLayer(Dia::Core::StringCRC("GV.CellState"));

    MockDebugDraw draw;
    mgr.Draw(draw);

    // CellState disabled → no rects; SightRadii has no sources → no circles;
    // Boundary starts disabled → no lines.
    EXPECT_EQ(draw.mRectCount, 0);
}

// ===========================================================================
// Suite: GridVisibilityDebugDomain_JSONState
// Covers AC D-7 (GetJSONState shape and content).
// ===========================================================================

TEST(GridVisibilityDebugDomain_JSONState, D7_DrawersArrayPresentWithThreeEntries)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    Json::Value state;
    f.domain.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(static_cast<int>(state["drawers"].size()), 3);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_TRUE(state["drawers"][i].isMember("name"))    << "drawers[" << i << "] missing 'name'";
        EXPECT_TRUE(state["drawers"][i].isMember("enabled")) << "drawers[" << i << "] missing 'enabled'";
    }
}

TEST(GridVisibilityDebugDomain_JSONState, D7_StatsObjectPresent)
{
    DomainFixture f;

    Json::Value state;
    f.domain.GetJSONState(state);

    ASSERT_TRUE(state["stats"].isObject());
    EXPECT_EQ(state["stats"]["gridWidth"].asInt(),        8);
    EXPECT_EQ(state["stats"]["gridHeight"].asInt(),       8);
    EXPECT_EQ(state["stats"]["chunkSize"].asInt(),        1);
    EXPECT_EQ(state["stats"]["groupCount"].asInt(),       0);
    EXPECT_EQ(state["stats"]["sightSourceCount"].asInt(), 0);
}

TEST(GridVisibilityDebugDomain_JSONState, D7_SelectedGroupPresentAsString)
{
    DomainFixture f;

    Json::Value state;
    f.domain.GetJSONState(state);

    EXPECT_TRUE(state["selectedGroup"].isString());
}

TEST(GridVisibilityDebugDomain_JSONState, D7_GroupsArrayPresentAndComplete)
{
    DomainFixture f;

    // Register two sight sources from different groups to populate mGroups.
    // Entity(index, generation) — both non-zero so IsValid() returns true.
    Dia::Entity::Entity e1(1u, 1u);
    Dia::Entity::Entity e2(2u, 1u);
    f.system->RegisterSightSource(e1, Dia::Core::StringCRC("Red"),  5.0f);
    f.system->RegisterSightSource(e2, Dia::Core::StringCRC("Blue"), 5.0f);

    MGR mgr;
    f.domain.Register(mgr);

    Json::Value state;
    f.domain.GetJSONState(state);

    ASSERT_TRUE(state["groups"].isArray());
    EXPECT_EQ(static_cast<int>(state["groups"].size()), 2);
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_TRUE(state["groups"][i].isMember("id"))             << "groups[" << i << "] missing 'id'";
        EXPECT_TRUE(state["groups"][i].isMember("visibleCells"))   << "groups[" << i << "] missing 'visibleCells'";
        EXPECT_TRUE(state["groups"][i].isMember("revealedCells"))  << "groups[" << i << "] missing 'revealedCells'";
        EXPECT_TRUE(state["groups"][i].isMember("unexploredCells"))<< "groups[" << i << "] missing 'unexploredCells'";
    }
}

TEST(GridVisibilityDebugDomain_JSONState, BoundaryDrawerReportsDisabledInJSON)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    Json::Value state;
    f.domain.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    ASSERT_EQ(static_cast<int>(state["drawers"].size()), 3);
    // drawers[2] = ShadowcastBoundaryDrawer — starts with SetEnabled(false)
    EXPECT_FALSE(state["drawers"][2]["enabled"].asBool());
}

// ===========================================================================
// Suite: GridVisibilityDebugDomain_OnCommand
// Covers AC D-8 (toggle + selectGroup commands) and robustness.
// ===========================================================================

TEST(GridVisibilityDebugDomain_OnCommand, D8_SelectGroupChangesState)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    Json::Value args;
    args["groupId"] = "Red";
    f.domain.OnCommand(Dia::Core::StringCRC("selectGroup"), args);

    Json::Value state;
    f.domain.GetJSONState(state);
    EXPECT_EQ(state["selectedGroup"].asString(), std::string("Red"));
}

TEST(GridVisibilityDebugDomain_OnCommand, ToggleCellStateFlipsLayer)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    // Initially enabled
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.CellState")));

    // First toggle → disabled
    {
        Json::Value args;
        args["drawer"] = "Cell State";
        f.domain.OnCommand(Dia::Core::StringCRC("toggle"), args);
    }
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.CellState")));

    // Second toggle → re-enabled
    {
        Json::Value args;
        args["drawer"] = "Cell State";
        f.domain.OnCommand(Dia::Core::StringCRC("toggle"), args);
    }
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.CellState")));
}

TEST(GridVisibilityDebugDomain_OnCommand, ToggleUnknownDrawerIsIgnored)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    Json::Value args;
    args["drawer"] = "NonExistentDrawer";
    f.domain.OnCommand(Dia::Core::StringCRC("toggle"), args);

    // CellState and SightRadii must remain enabled
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.CellState")));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.SightRadii")));
}

TEST(GridVisibilityDebugDomain_OnCommand, MalformedCommandIsIgnored)
{
    MGR mgr;
    DomainFixture f;
    f.domain.Register(mgr);

    // Empty args — no "drawer" key → early return, no crash
    Json::Value emptyArgs;
    f.domain.OnCommand(Dia::Core::StringCRC("toggle"), emptyArgs);

    // Layers unchanged
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.CellState")));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Core::StringCRC("GV.SightRadii")));
}

TEST(GridVisibilityDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    DomainFixture f;
    // No Register call — mLayerManager is nullptr

    {
        Json::Value args;
        args["drawer"] = "Cell State";
        f.domain.OnCommand(Dia::Core::StringCRC("toggle"), args);  // must not crash
    }
    {
        Json::Value args;
        args["groupId"] = "Red";
        f.domain.OnCommand(Dia::Core::StringCRC("selectGroup"), args);  // must not crash
    }
}

#endif // DIA_DEBUG
