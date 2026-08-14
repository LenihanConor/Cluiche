////////////////////////////////////////////////////////////////////////////////
// TestFlowFieldVisualDebugger.cpp
// Tests for FlowFieldVisualDebugger — 17 tests across 4 suites.
// AC-15 (mandatory shapes) covered: DrawerGate, PrimitiveType, ScaleSensitivity,
//   JSONRoundTrip, OnCommandRoundTrip.
// Domain-specific shapes: direction counts, reachability colours, stats accuracy.
// System spec: docs/specs/applications/dia/systems/diaflowfieldvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaFlowFieldVisualDebugger/FlowFieldVisualDebugger.h>
#include <DiaFlowFieldVisualDebugger/DirectionArrowsDrawer.h>
#include <DiaFlowFieldVisualDebugger/ReachabilityOverlayDrawer.h>
#include <DiaFlowField/FlowField.h>
#include <DiaFlowField/FlowCell.h>
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
#include <cmath>

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
    struct LineCapture { Dia::Maths::Vector2D start, end; Dia::Core::RGBA colour; };
    struct RectCapture { Dia::Maths::Vector2D min, max; Dia::Core::RGBA outline; Dia::Core::RGBA fill; };

    std::vector<LineCapture> lines;
    std::vector<RectCapture> rects;

    void RequestDraw(const Dia::Maths::Vector2D& start, const Dia::Maths::Vector2D& end,
                     Dia::Core::RGBA colour) override
    { lines.push_back({start, end, colour}); }

    void RequestDraw(const Dia::Maths::Vector2D&, float,
                     Dia::Core::RGBA, Dia::Core::RGBA) override {}

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

// Mark R reachable cells in a W x H field with a given direction.
void MarkReachable(Dia::FlowField::FlowField& field, int count,
                   Dia::Maths::Vector2D dir = Dia::Maths::Vector2D(1.0f, 0.0f))
{
    int marked = 0;
    for (int row = 0; row < field.GetHeight() && marked < count; ++row)
        for (int col = 0; col < field.GetWidth() && marked < count; ++col)
        {
            auto& cell      = field.AccessCell(Dia::Pathfinding::CellCoord{col, row});
            cell.reachable  = true;
            cell.direction  = dir;
            ++marked;
        }
}

} // anonymous namespace

// ===========================================================================
// Suite: FlowFieldVisualDebugger_Identity
// ===========================================================================

// AC-15: world-space domain with exactly 2 drawers.
TEST(FlowFieldVisualDebugger_Identity, PrimitiveType_HasWorldDrawers)
{
    Dia::FlowField::FlowField field(4, 4);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    EXPECT_TRUE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 2);
    // Drawers allocated lazily in Register(); nullptr before that call.
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
    EXPECT_EQ(domain.GetDrawer(1), nullptr);
}

// AC-15: DirectionArrows SetEnabled(false) emits zero lines; re-enable restores.
TEST(FlowFieldVisualDebugger_Identity, DrawerGate)
{
    Dia::FlowField::FlowField field(3, 3);
    MarkReachable(field, 5);
    StubDebugContext ctx;
    Dia::FlowField::DirectionArrowsDrawer drawer(field, ctx, 10.0f);

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

// AC-15: DirectionArrows → line primitives; ReachabilityOverlay → quad primitives.
TEST(FlowFieldVisualDebugger_Identity, PrimitiveType)
{
    Dia::FlowField::FlowField field(3, 3);
    MarkReachable(field, 4);
    StubDebugContext ctx;

    {
        Dia::FlowField::DirectionArrowsDrawer drawer(field, ctx, 10.0f);
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.lines.size(), 0u) << "DirectionArrows must emit line primitives";
        EXPECT_EQ(draw.rects.size(), 0u) << "DirectionArrows must not emit rect primitives";
    }
    {
        Dia::FlowField::ReachabilityOverlayDrawer drawer(field, ctx, 10.0f);
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.rects.size(), 0u) << "ReachabilityOverlay must emit rect primitives";
        EXPECT_EQ(draw.lines.size(), 0u) << "ReachabilityOverlay must not emit line primitives";
    }
}

// AC-15: doubling GetDebugScale() doubles arrow lengths.
TEST(FlowFieldVisualDebugger_Identity, ScaleSensitivity)
{
    Dia::FlowField::FlowField field(2, 1);
    MarkReachable(field, 2, Dia::Maths::Vector2D(1.0f, 0.0f));
    StubDebugContext ctx;

    Dia::FlowField::DirectionArrowsDrawer drawer(field, ctx, 10.0f);

    ctx.scale = 1.0f;
    MockDraw d1;
    drawer.Draw(d1);
    ASSERT_GE(d1.lines.size(), 1u);
    const float len1 = d1.lines[0].end.x - d1.lines[0].start.x;

    ctx.scale = 2.0f;
    MockDraw d2;
    drawer.Draw(d2);
    ASSERT_GE(d2.lines.size(), 1u);
    const float len2 = d2.lines[0].end.x - d2.lines[0].start.x;

    EXPECT_NEAR(len2, len1 * 2.0f, 0.001f)
        << "Arrow length must scale linearly with GetDebugScale()";
}

// AC-15: GetJSONState drawer entries match current enabled state.
TEST(FlowFieldVisualDebugger_Identity, JSONRoundTrip)
{
    Dia::FlowField::FlowField field(4, 4);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    Json::Value s1;
    domain.GetJSONState(s1);
    ASSERT_TRUE(s1.isMember("drawers"));
    ASSERT_EQ(s1["drawers"].size(), 2u);
    EXPECT_STREQ(s1["drawers"][0u]["name"].asCString(), "DirectionArrows");
    EXPECT_TRUE(s1["drawers"][0u]["enabled"].asBool());
    EXPECT_STREQ(s1["drawers"][1u]["name"].asCString(), "ReachabilityOverlay");
    EXPECT_TRUE(s1["drawers"][1u]["enabled"].asBool());

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("DirectionArrows"));
    Json::Value s2;
    domain.GetJSONState(s2);
    EXPECT_FALSE(s2["drawers"][0u]["enabled"].asBool())
        << "DirectionArrows must be disabled after toggle";
}

// AC-15: two toggles restore the original enabled state.
TEST(FlowFieldVisualDebugger_Identity, OnCommandRoundTrip)
{
    Dia::FlowField::FlowField field(4, 4);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("DirectionArrows"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_FALSE(s["drawers"][0u]["enabled"].asBool());
    }

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("DirectionArrows"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_TRUE(s["drawers"][0u]["enabled"].asBool());
    }
}

// ===========================================================================
// Suite: FlowFieldVisualDebugger_DirectionArrows
// ===========================================================================

// R reachable cells → R line primitives.
TEST(FlowFieldVisualDebugger_DirectionArrows, OneArrowPerReachableCell)
{
    Dia::FlowField::FlowField field(4, 4);
    MarkReachable(field, 7);
    StubDebugContext ctx;
    Dia::FlowField::DirectionArrowsDrawer drawer(field, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.lines.size(), 7u) << "Each reachable cell must produce exactly one arrow";
}

// Unreachable cells produce no arrow.
TEST(FlowFieldVisualDebugger_DirectionArrows, NoArrowForUnreachableCell)
{
    Dia::FlowField::FlowField field(3, 3); // all cells default to unreachable
    StubDebugContext ctx;
    Dia::FlowField::DirectionArrowsDrawer drawer(field, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.lines.size(), 0u) << "Unreachable cells must not produce arrows";
}

// Arrow endpoint direction matches FlowCell.direction (within float tolerance).
TEST(FlowFieldVisualDebugger_DirectionArrows, ArrowDirectionMatchesFlowCell)
{
    Dia::FlowField::FlowField field(1, 1);
    const Dia::Maths::Vector2D dir(0.0f, 1.0f); // pointing up
    MarkReachable(field, 1, dir);
    StubDebugContext ctx;
    ctx.scale = 1.0f;
    const float cellSize = 10.0f;
    Dia::FlowField::DirectionArrowsDrawer drawer(field, ctx, cellSize);

    MockDraw draw;
    drawer.Draw(draw);

    ASSERT_EQ(draw.lines.size(), 1u);
    const float dx = draw.lines[0].end.x - draw.lines[0].start.x;
    const float dy = draw.lines[0].end.y - draw.lines[0].start.y;
    const float mag = std::sqrt(dx * dx + dy * dy);
    ASSERT_GT(mag, 0.001f);

    // Normalised direction must match FlowCell.direction.
    EXPECT_NEAR(dx / mag, dir.X(), 0.001f);
    EXPECT_NEAR(dy / mag, dir.Y(), 0.001f);
}

// OnCommand setScale "arrowLength" with value 2.0 doubles the arrow length.
TEST(FlowFieldVisualDebugger_DirectionArrows, ArrowLength_Command)
{
    Dia::FlowField::FlowField field(1, 1);
    MarkReachable(field, 1, Dia::Maths::Vector2D(1.0f, 0.0f));
    StubDebugContext ctx;
    ctx.scale = 1.0f;
    Dia::FlowField::DirectionArrowsDrawer drawer(field, ctx, 10.0f);

    MockDraw d1;
    drawer.Draw(d1);
    ASSERT_EQ(d1.lines.size(), 1u);
    const float len1 = d1.lines[0].end.x - d1.lines[0].start.x;

    drawer.SetArrowLengthScale(2.0f);
    MockDraw d2;
    drawer.Draw(d2);
    ASSERT_EQ(d2.lines.size(), 1u);
    const float len2 = d2.lines[0].end.x - d2.lines[0].start.x;

    EXPECT_NEAR(len2, len1 * 2.0f, 0.001f)
        << "Arrow length must double after SetArrowLengthScale(2.0f)";
}

// ===========================================================================
// Suite: FlowFieldVisualDebugger_ReachabilityOverlay
// ===========================================================================

// N-cell field → N quad primitives from overlay.
TEST(FlowFieldVisualDebugger_ReachabilityOverlay, OneCellPerAllCells)
{
    Dia::FlowField::FlowField field(4, 4);
    StubDebugContext ctx;
    Dia::FlowField::ReachabilityOverlayDrawer drawer(field, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.rects.size(), 16u) << "4x4 field must produce 16 overlay quads";
}

// Reachable and unreachable cells use different fill colours.
TEST(FlowFieldVisualDebugger_ReachabilityOverlay, ColoursDiffer)
{
    Dia::FlowField::FlowField field(2, 1);
    field.AccessCell(Dia::Pathfinding::CellCoord{0, 0}).reachable = true;
    // cell {1,0} stays unreachable
    StubDebugContext ctx;
    Dia::FlowField::ReachabilityOverlayDrawer drawer(field, ctx, 10.0f);

    MockDraw draw;
    drawer.Draw(draw);

    ASSERT_EQ(draw.rects.size(), 2u);
    EXPECT_NE(draw.rects[0].fill, draw.rects[1].fill)
        << "Reachable and unreachable cells must use distinct fill colours";
}

// SetEnabled(false) suppresses all quads.
TEST(FlowFieldVisualDebugger_ReachabilityOverlay, DrawerGate_ReachabilityOverlay)
{
    Dia::FlowField::FlowField field(3, 3);
    StubDebugContext ctx;
    Dia::FlowField::ReachabilityOverlayDrawer drawer(field, ctx, 10.0f);

    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.rects.size(), 9u);
    }

    drawer.SetEnabled(false);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.rects.size(), 0u);
    }
}

// OnCommand toggle for ReachabilityOverlay toggles it.
TEST(FlowFieldVisualDebugger_ReachabilityOverlay, Toggle_ReachabilityOverlay)
{
    Dia::FlowField::FlowField field(3, 3);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ReachabilityOverlay"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_STREQ(s["drawers"][1u]["name"].asCString(), "ReachabilityOverlay");
        EXPECT_FALSE(s["drawers"][1u]["enabled"].asBool());
    }

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("ReachabilityOverlay"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_TRUE(s["drawers"][1u]["enabled"].asBool());
    }
}

// ===========================================================================
// Suite: FlowFieldVisualDebugger_Stats
// ===========================================================================

// stats.cellCount equals FlowField::GetCellCount().
TEST(FlowFieldVisualDebugger_Stats, CellCount_Accurate)
{
    Dia::FlowField::FlowField field(5, 3);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    Json::Value s;
    domain.GetJSONState(s);

    EXPECT_EQ(s["stats"]["cellCount"].asInt(), 15);
}

// stats.reachableCount matches the actual count of reachable cells.
TEST(FlowFieldVisualDebugger_Stats, ReachableCount_Accurate)
{
    Dia::FlowField::FlowField field(4, 4);
    MarkReachable(field, 9);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    Json::Value s;
    domain.GetJSONState(s);

    EXPECT_EQ(s["stats"]["reachableCount"].asInt(), 9);
}

// stats.isComplete mirrors FlowField::IsComplete().
TEST(FlowFieldVisualDebugger_Stats, IsComplete_Mirrors_Field)
{
    Dia::FlowField::FlowField field(2, 2);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_FALSE(s["stats"]["isComplete"].asBool())
            << "isComplete must be false when cells are unreachable";
    }

    // Mark all 4 cells reachable.
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 2; ++col)
            field.AccessCell(Dia::Pathfinding::CellCoord{col, row}).reachable = true;

    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_TRUE(s["stats"]["isComplete"].asBool())
            << "isComplete must be true when all cells are reachable";
    }
}

// 0-cell field (0x0 default) — no output, no crash.
TEST(FlowFieldVisualDebugger_Stats, EmptyField_ZeroCells_NoAssert)
{
    // FlowField(0,0) would trip DIA_ASSERT (0*0 <= 4096 is fine, but no cells).
    // Use the smallest legal field instead (1x1, all unreachable).
    Dia::FlowField::FlowField field(1, 1);
    Dia::FlowField::FlowFieldVisualDebugger domain(field, 10.0f);

    StubDebugContext ctx;
    Dia::FlowField::DirectionArrowsDrawer arrows(field, ctx, 10.0f);
    Dia::FlowField::ReachabilityOverlayDrawer overlay(field, ctx, 10.0f);

    MockDraw draw;
    EXPECT_NO_FATAL_FAILURE(arrows.Draw(draw));
    EXPECT_EQ(draw.lines.size(), 0u);

    MockDraw draw2;
    EXPECT_NO_FATAL_FAILURE(overlay.Draw(draw2));
    EXPECT_EQ(draw2.rects.size(), 1u); // 1 cell, unreachable
}

#endif // DIA_DEBUG
