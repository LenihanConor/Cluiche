////////////////////////////////////////////////////////////////////////////////
// TestPathfindingVisualDebugger.cpp
// Tests for PathfindingVisualDebugger — 18 tests across 5 suites.
// AC-15 (mandatory shapes) covered: DrawerGate, PrimitiveType, ScaleSensitivity,
//   JSONRoundTrip, OnCommandRoundTrip.
// Domain-specific shapes: PathPolyline counts, start/goal markers, grid overlay.
// System spec: docs/specs/applications/dia/systems/diapathfindingvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaPathfindingVisualDebugger/PathfindingVisualDebugger.h>
#include <DiaPathfindingVisualDebugger/PathPolylineDrawer.h>
#include <DiaPathfindingVisualDebugger/GridPassabilityDrawer.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/PathResult.h>
#include <DiaPathfinding/CellCoord.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <vector>

namespace
{

// ─── Stubs & Mocks ──────────────────────────────────────────────────────────

struct StubDebugContext : Dia::Core::IDebugContext
{
    float    scale      = 1.0f;
    uint32_t selectedId = 0u;

    float    GetDebugScale()                   const override { return scale; }
    uint32_t GetSelectedEntityId()             const override { return selectedId; }
    void     SetSelectedEntityId(uint32_t id)        override { selectedId = id; }
};

struct MockDraw : Dia::Core::IDebugDraw
{
    struct LineCapture   { Dia::Maths::Vector2D start, end; Dia::Core::RGBA colour; };
    struct CircleCapture { Dia::Maths::Vector2D pos; float radius; Dia::Core::RGBA outline; Dia::Core::RGBA fill; };
    struct RectCapture   { Dia::Maths::Vector2D min, max; Dia::Core::RGBA outline; Dia::Core::RGBA fill; };

    std::vector<LineCapture>   lines;
    std::vector<CircleCapture> circles;
    std::vector<RectCapture>   rects;

    void RequestDraw(const Dia::Maths::Vector2D& start, const Dia::Maths::Vector2D& end,
                     Dia::Core::RGBA colour) override
    { lines.push_back({start, end, colour}); }

    void RequestDraw(const Dia::Maths::Vector2D& pos, float radius,
                     Dia::Core::RGBA outline, Dia::Core::RGBA fill) override
    { circles.push_back({pos, radius, outline, fill}); }

    void RequestDrawRect(const Dia::Maths::Vector2D& min, const Dia::Maths::Vector2D& max,
                         Dia::Core::RGBA outline, Dia::Core::RGBA fill) override
    { rects.push_back({min, max, outline, fill}); }

    void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}
    void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float, Dia::Core::RGBA) override {}
    void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, float, Dia::Core::RGBA) override {}
    void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                     Dia::Core::RGBA, Dia::Core::RGBA) override {}
    void RequestDrawText(const Dia::Maths::Vector2D&, const char*, float, Dia::Core::RGBA) override {}
    void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
    void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
    void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
    void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
    void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, float, Dia::Core::RGBA) override {}
    uint32_t DroppedCount() const override { return 0u; }
    const Dia::Maths::Vector2D& GetMousePixel() const override
    {
        static const Dia::Maths::Vector2D kZero(0.0f, 0.0f);
        return kZero;
    }
};

// ─── Helpers ────────────────────────────────────────────────────────────────

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args(Json::objectValue);
    args["drawer"] = drawerName;
    return args;
}

Json::Value ScaleArgs(const char* key, float value)
{
    Json::Value args(Json::objectValue);
    args["key"]   = key;
    args["value"] = value;
    return args;
}

// Build a PathResult with N cells along the x-axis (cells[i] = {i, 0}).
Dia::Pathfinding::PathResult MakeLinearPath(int numCells, float costPerStep = 1.0f)
{
    Dia::Pathfinding::PathResult r;
    r.success = (numCells >= 2);
    r.totalCost = static_cast<float>(numCells - 1) * costPerStep;
    for (int i = 0; i < numCells; ++i)
        r.cells.Add(Dia::Pathfinding::CellCoord{ i, 0 });
    return r;
}

} // anonymous namespace

// ===========================================================================
// Suite: PathfindingVisualDebugger_Identity
// ===========================================================================

// AC-15: world-space domain with exactly 2 drawers.
TEST(PathfindingVisualDebugger_Identity, PrimitiveType_HasWorldDrawers)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    Dia::Pathfinding::PathResult result;
    Dia::Pathfinding::PathfindingVisualDebugger domain(grid, result, 10.0f);

    EXPECT_TRUE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 2);
    // Drawers are allocated lazily in Register(); nullptr before that call.
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
    EXPECT_EQ(domain.GetDrawer(1), nullptr);
}

// AC-15: SetEnabled(false) suppresses all draw calls; re-enable restores them.
TEST(PathfindingVisualDebugger_Identity, DrawerGate)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    auto result = MakeLinearPath(3);
    StubDebugContext ctx;
    Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);

    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.lines.size(), 0u);
    }

    drawer.SetEnabled(false);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.lines.size(), 0u);
    }

    drawer.SetEnabled(true);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.lines.size(), 0u);
    }
}

// AC-15: PathPolylineDrawer calls RequestDraw(line) and RequestDraw(circle);
//         GridPassabilityDrawer calls RequestDrawRect.
TEST(PathfindingVisualDebugger_Identity, PrimitiveType)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    auto result = MakeLinearPath(3);
    StubDebugContext ctx;

    {
        Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.lines.size(), 0u)   << "PathPolylineDrawer must call RequestDraw(line)";
        EXPECT_GT(draw.circles.size(), 0u) << "PathPolylineDrawer must call RequestDraw(circle)";
    }
    {
        Dia::Pathfinding::GridPassabilityDrawer drawer(grid, ctx, 10.0f);
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.rects.size(), 0u) << "GridPassabilityDrawer must call RequestDrawRect";
    }
}

// AC-15: marker radius and grid cell size both scale with IDebugContext::GetDebugScale().
TEST(PathfindingVisualDebugger_Identity, ScaleSensitivity)
{
    Dia::Pathfinding::SquarePathGrid grid(2, 1);
    auto result = MakeLinearPath(2);
    StubDebugContext ctx;

    // Marker radius doubles when debug scale doubles.
    {
        Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);
        ctx.scale = 1.0f;
        MockDraw d1; drawer.Draw(d1);
        ASSERT_EQ(d1.circles.size(), 2u);
        const float R1 = d1.circles[0].radius;

        ctx.scale = 2.0f;
        MockDraw d2; drawer.Draw(d2);
        ASSERT_EQ(d2.circles.size(), 2u);
        const float R2 = d2.circles[0].radius;

        EXPECT_NEAR(R2, R1 * 2.0f, 0.001f)
            << "Marker radius must scale linearly with GetDebugScale()";
    }

    // Grid cell rect size doubles when debug scale doubles.
    {
        Dia::Pathfinding::GridPassabilityDrawer drawer(grid, ctx, 10.0f);
        ctx.scale = 1.0f;
        MockDraw d1; drawer.Draw(d1);
        ASSERT_GE(d1.rects.size(), 1u);
        const float w1 = d1.rects[0].max.x - d1.rects[0].min.x;

        ctx.scale = 2.0f;
        MockDraw d2; drawer.Draw(d2);
        ASSERT_GE(d2.rects.size(), 1u);
        const float w2 = d2.rects[0].max.x - d2.rects[0].min.x;

        EXPECT_NEAR(w2, w1 * 2.0f, 0.001f)
            << "Grid cell rect width must scale linearly with GetDebugScale()";
    }
}

// AC-15: GetJSONState reports correct drawer names and default enabled states.
TEST(PathfindingVisualDebugger_Identity, JSONRoundTrip)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    auto result = MakeLinearPath(5);
    Dia::Pathfinding::PathfindingVisualDebugger domain(grid, result, 10.0f);

    Json::Value s1;
    domain.GetJSONState(s1);
    ASSERT_TRUE(s1.isMember("drawers"));
    ASSERT_EQ(s1["drawers"].size(), 2u);
    EXPECT_STREQ(s1["drawers"][0u]["name"].asCString(), "PathPolyline");
    EXPECT_TRUE(s1["drawers"][0u]["enabled"].asBool());
    EXPECT_STREQ(s1["drawers"][1u]["name"].asCString(), "GridPassability");
    EXPECT_TRUE(s1["drawers"][1u]["enabled"].asBool());

    // Toggle PathPolyline off and verify.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("PathPolyline"));
    Json::Value s2;
    domain.GetJSONState(s2);
    EXPECT_FALSE(s2["drawers"][0u]["enabled"].asBool())
        << "PathPolyline must be disabled after toggle";
}

// AC-15: two toggles restore the original enabled state.
TEST(PathfindingVisualDebugger_Identity, OnCommandRoundTrip)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    Dia::Pathfinding::PathResult result;
    Dia::Pathfinding::PathfindingVisualDebugger domain(grid, result, 10.0f);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("GridPassability"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_FALSE(s["drawers"][1u]["enabled"].asBool());
    }

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("GridPassability"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_TRUE(s["drawers"][1u]["enabled"].asBool());
    }
}

// ===========================================================================
// Suite: PathfindingVisualDebugger_PathPolyline
// ===========================================================================

// N waypoints → N-1 line segments + 2 marker circles.
TEST(PathfindingVisualDebugger_PathPolyline, NWaypoints_NMinusOneSegments)
{
    Dia::Pathfinding::SquarePathGrid grid(8, 4);
    auto result = MakeLinearPath(5);
    StubDebugContext ctx;
    Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.lines.size(), 4u)   << "5 waypoints must produce 4 segments";
    EXPECT_EQ(draw.circles.size(), 2u) << "5 waypoints must produce 2 marker circles";
}

// Start marker is drawn at the world position of the first waypoint.
TEST(PathfindingVisualDebugger_PathPolyline, StartMarker_AtFirstWaypoint)
{
    Dia::Pathfinding::SquarePathGrid grid(8, 4);
    auto result = MakeLinearPath(3);
    StubDebugContext ctx;
    Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    ASSERT_EQ(draw.circles.size(), 2u);
    const Dia::Maths::Vector2D expectedStart(0.0f, 0.0f); // cell {0,0} * cellSize 10
    EXPECT_NEAR(draw.circles[0].pos.x, expectedStart.x, 0.001f);
    EXPECT_NEAR(draw.circles[0].pos.y, expectedStart.y, 0.001f);
}

// Goal marker is drawn at the world position of the last waypoint.
TEST(PathfindingVisualDebugger_PathPolyline, GoalMarker_AtLastWaypoint)
{
    Dia::Pathfinding::SquarePathGrid grid(8, 4);
    auto result = MakeLinearPath(3); // cells: {0,0},{1,0},{2,0} with cellSize=10 → (0,0),(10,0),(20,0)
    StubDebugContext ctx;
    Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    ASSERT_EQ(draw.circles.size(), 2u);
    const Dia::Maths::Vector2D expectedGoal(20.0f, 0.0f);
    EXPECT_NEAR(draw.circles[1].pos.x, expectedGoal.x, 0.001f);
    EXPECT_NEAR(draw.circles[1].pos.y, expectedGoal.y, 0.001f);
}

// Start and goal markers use distinct colours.
TEST(PathfindingVisualDebugger_PathPolyline, StartGoalColoursDiffer)
{
    Dia::Pathfinding::SquarePathGrid grid(8, 4);
    auto result = MakeLinearPath(2);
    StubDebugContext ctx;
    Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    ASSERT_EQ(draw.circles.size(), 2u);
    EXPECT_NE(draw.circles[0].outline, draw.circles[1].outline)
        << "Start and goal markers must use distinct colours";
}

// Empty / failed path produces no primitives and does not crash.
TEST(PathfindingVisualDebugger_PathPolyline, NoActivePath_NoSegments_NoAssert)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    Dia::Pathfinding::PathResult result; // success = false
    StubDebugContext ctx;
    Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);

    MockDraw draw;
    EXPECT_NO_FATAL_FAILURE(drawer.Draw(draw));
    EXPECT_EQ(draw.lines.size(),   0u);
    EXPECT_EQ(draw.circles.size(), 0u);
}

// ===========================================================================
// Suite: PathfindingVisualDebugger_GridPassability
// ===========================================================================

// 4×4 grid → exactly 16 quads drawn.
TEST(PathfindingVisualDebugger_GridPassability, CellsMatchGrid)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    Dia::Pathfinding::PathResult result;
    StubDebugContext ctx;
    Dia::Pathfinding::GridPassabilityDrawer drawer(grid, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.rects.size(), 16u) << "4x4 grid must produce 16 quads";
}

// Passable and impassable cells use distinct fill colours.
TEST(PathfindingVisualDebugger_GridPassability, PassableImpassableColoursDiffer)
{
    Dia::Pathfinding::SquarePathGrid grid(2, 1);
    grid.SetPassable(Dia::Pathfinding::CellCoord{0, 0}, false); // impassable
    // cell {1,0} stays passable
    Dia::Pathfinding::PathResult result;
    StubDebugContext ctx;
    Dia::Pathfinding::GridPassabilityDrawer drawer(grid, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    ASSERT_EQ(draw.rects.size(), 2u);
    EXPECT_NE(draw.rects[0].fill, draw.rects[1].fill)
        << "Impassable and passable cells must use distinct fill colours";
}

// SetEnabled(false) on GridPassabilityDrawer suppresses all rects.
TEST(PathfindingVisualDebugger_GridPassability, DrawerGate_GridPassability)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    Dia::Pathfinding::PathResult result;
    StubDebugContext ctx;
    Dia::Pathfinding::GridPassabilityDrawer drawer(grid, ctx, 10.0f);

    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.rects.size(), 16u);
    }

    drawer.SetEnabled(false);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.rects.size(), 0u);
    }
}

// OnCommand("toggle", {drawer:"GridPassability"}) toggles the enabled flag.
TEST(PathfindingVisualDebugger_GridPassability, Toggle_GridPassability)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    Dia::Pathfinding::PathResult result;
    Dia::Pathfinding::PathfindingVisualDebugger domain(grid, result, 10.0f);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("GridPassability"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_STREQ(s["drawers"][1u]["name"].asCString(), "GridPassability");
        EXPECT_FALSE(s["drawers"][1u]["enabled"].asBool());
    }

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("GridPassability"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_TRUE(s["drawers"][1u]["enabled"].asBool());
    }
}

// ===========================================================================
// Suite: PathfindingVisualDebugger_Stats
// ===========================================================================

// stats.pathActive is false when PathResult.success is false.
TEST(PathfindingVisualDebugger_Stats, PathActive_False_WhenNoPath)
{
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    Dia::Pathfinding::PathResult result; // success = false
    Dia::Pathfinding::PathfindingVisualDebugger domain(grid, result, 10.0f);

    Json::Value s;
    domain.GetJSONState(s);

    EXPECT_FALSE(s["stats"]["pathActive"].asBool());
}

// stats.waypointCount matches the number of cells in PathResult.
TEST(PathfindingVisualDebugger_Stats, WaypointCount_Accurate)
{
    Dia::Pathfinding::SquarePathGrid grid(8, 4);
    auto result = MakeLinearPath(7);
    Dia::Pathfinding::PathfindingVisualDebugger domain(grid, result, 10.0f);

    Json::Value s;
    domain.GetJSONState(s);

    EXPECT_EQ(s["stats"]["waypointCount"].asInt(), 7);
}

// stats.totalCost matches PathResult.totalCost.
TEST(PathfindingVisualDebugger_Stats, TotalCost_Accurate)
{
    Dia::Pathfinding::SquarePathGrid grid(8, 4);
    auto result = MakeLinearPath(4, 3.5f); // 3 steps * 3.5 = 10.5
    Dia::Pathfinding::PathfindingVisualDebugger domain(grid, result, 10.0f);

    Json::Value s;
    domain.GetJSONState(s);

    EXPECT_NEAR(s["stats"]["totalCost"].asFloat(), 10.5f, 0.001f);
}

// setScale "markerRadius" doubles the drawn circle radius.
TEST(PathfindingVisualDebugger_Stats, MarkerRadius_Command)
{
    Dia::Pathfinding::SquarePathGrid grid(8, 4);
    auto result = MakeLinearPath(2);
    StubDebugContext ctx;
    ctx.scale = 1.0f;
    Dia::Pathfinding::PathPolylineDrawer drawer(grid, result, ctx, 10.0f);

    MockDraw d1;
    drawer.Draw(d1);
    ASSERT_EQ(d1.circles.size(), 2u);
    const float R1 = d1.circles[0].radius;

    drawer.SetMarkerRadius(2.0f * 5.0f); // default mMarkerRadius is 5.0, so set to 10.0

    MockDraw d2;
    drawer.Draw(d2);
    ASSERT_EQ(d2.circles.size(), 2u);
    const float R2 = d2.circles[0].radius;

    EXPECT_NEAR(R2, R1 * 2.0f, 0.001f)
        << "Marker radius must double after SetMarkerRadius(10.0f) from default 5.0f";
}

#endif // DIA_DEBUG
