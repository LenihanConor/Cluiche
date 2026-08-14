////////////////////////////////////////////////////////////////////////////////
// TestSteeringVisualDebugger.cpp
// Tests for SteeringVisualDebugger — 18 tests across 5 suites.
// AC-15 (mandatory shapes) covered: DrawerGate, PrimitiveType, ScaleSensitivity,
//   JSONRoundTrip, OnCommandRoundTrip.
// Domain-specific shapes: Arrows (2 per agent), SeparationRadius, DetectionBoxes.
// System spec: docs/specs/applications/dia/systems/diasteeringvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaSteeringVisualDebugger/SteeringVisualDebugger.h>
#include <DiaSteeringVisualDebugger/VelocityArrowsDrawer.h>
#include <DiaSteeringVisualDebugger/SeparationRadiusDrawer.h>
#include <DiaSteeringVisualDebugger/DetectionBoxDrawer.h>
#include <DiaSteering/SteeringSystem.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaSteering/SteeringPipeline.h>
#include <DiaSteering/Behaviours.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <utility>
#include <vector>
#include <cmath>

namespace
{

// ─── Stubs & Mocks ──────────────────────────────────────────────────────────

// Stub IDebugContext with settable scale.
struct StubDebugContext : Dia::Core::IDebugContext
{
    float    scale      = 1.0f;
    uint32_t selectedId = 0u;

    float    GetDebugScale()              const override { return scale; }
    uint32_t GetSelectedEntityId()        const override { return selectedId; }
    void     SetSelectedEntityId(uint32_t id) override  { selectedId = id; }
};

// Mock IDebugDraw — counts and captures the primitives that drawers submit.
// Only overrides the pure-virtual 4-arg overloads; the 3-arg convenience
// overloads in IDebugDraw call through to these automatically.
struct MockDraw : Dia::Core::IDebugDraw
{
    struct RayCapture    { Dia::Core::RGBA colour; float length; };
    struct CircleCapture { float radius; };
    struct RectCapture   { Dia::Maths::Vector2D min, max; };

    std::vector<RayCapture>    rays;
    std::vector<CircleCapture> circles;
    std::vector<RectCapture>   rects;

    // Circle2D with fill (pure virtual) — SeparationRadius calls the 3-arg
    // outline-only convenience which delegates here.
    void RequestDraw(const Dia::Maths::Vector2D& /*pos*/, float radius,
                     Dia::Core::RGBA /*outline*/, Dia::Core::RGBA /*fill*/) override
    { circles.push_back({radius}); }

    // Line2D (pure virtual)
    void RequestDraw(const Dia::Maths::Vector2D& /*start*/,
                     const Dia::Maths::Vector2D& /*end*/,
                     Dia::Core::RGBA /*colour*/) override {}

    // Point2D (pure virtual)
    void RequestDrawPoint(const Dia::Maths::Vector2D& /*pos*/,
                          Dia::Core::RGBA /*colour*/) override {}

    // Rect2D with fill (pure virtual) — DetectionBoxes calls the 3-arg
    // outline-only convenience which delegates here.
    void RequestDrawRect(const Dia::Maths::Vector2D& min,
                         const Dia::Maths::Vector2D& max,
                         Dia::Core::RGBA /*outline*/,
                         Dia::Core::RGBA /*fill*/) override
    { rects.push_back({min, max}); }

    // Arc2D (pure virtual)
    void RequestDrawArc(const Dia::Maths::Vector2D& /*pos*/, float /*radius*/,
                        float /*startDeg*/, float /*endDeg*/,
                        Dia::Core::RGBA /*colour*/) override {}

    // Ray2D (pure virtual) — VelocityArrows uses this.
    void RequestDrawRay(const Dia::Maths::Vector2D& /*origin*/,
                        const Dia::Maths::Vector2D& /*dir*/,
                        float length, Dia::Core::RGBA colour) override
    { rays.push_back({colour, length}); }

    // Triangle2D with fill (pure virtual)
    void RequestDraw(const Dia::Maths::Vector2D& /*p1*/,
                     const Dia::Maths::Vector2D& /*p2*/,
                     const Dia::Maths::Vector2D& /*p3*/,
                     Dia::Core::RGBA /*outline*/,
                     Dia::Core::RGBA /*fill*/) override {}

    // Text2D (pure virtual)
    void RequestDrawText(const Dia::Maths::Vector2D& /*pos*/,
                         const char* /*text*/,
                         float /*fontSize*/,
                         Dia::Core::RGBA /*colour*/) override {}

    // 3-D draw methods (pure virtual)
    void RequestDrawLine3D(const Dia::Maths::Vector3D& /*from*/,
                           const Dia::Maths::Vector3D& /*to*/,
                           Dia::Core::RGBA /*colour*/) override {}

    void RequestDrawRay3D(const Dia::Maths::Vector3D& /*origin*/,
                          const Dia::Maths::Vector3D& /*dir*/,
                          float /*length*/, Dia::Core::RGBA /*colour*/) override {}

    void RequestDrawBox3D(const Dia::Maths::Vector3D& /*min*/,
                          const Dia::Maths::Vector3D& /*max*/,
                          Dia::Core::RGBA /*colour*/) override {}

    void RequestDrawSphere3D(const Dia::Maths::Vector3D& /*center*/,
                              float /*radius*/,
                              Dia::Core::RGBA /*colour*/) override {}

    void RequestDrawArrow3D(const Dia::Maths::Vector3D& /*origin*/,
                             const Dia::Maths::Vector3D& /*dir*/,
                             float /*length*/, float /*headSize*/,
                             Dia::Core::RGBA /*colour*/) override {}

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

// Hardcoded agent IDs — avoids std::to_string dependency.
static const char* kAgentIds[] = { "a0", "a1", "a2", "a3" };

} // anonymous namespace

// ===========================================================================
// Suite: SteeringVisualDebugger_Identity
// ===========================================================================

// AC-15: world-space domain with exactly 3 drawers.
TEST(SteeringVisualDebugger_Identity, PrimitiveType_HasWorldDrawers)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringVisualDebugger domain(system);

    EXPECT_TRUE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 3);
    // Drawers are allocated lazily in Register(); nullptr before that call.
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

// AC-15: SetEnabled(false) suppresses all draw calls; SetEnabled(true) restores them.
TEST(SteeringVisualDebugger_Identity, DrawerGate)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    agent.velocity = Dia::Maths::Vector2D(2.0f, 0.0f);
    agent.maxSpeed = 5.0f;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);

    StubDebugContext ctx;
    Dia::Steering::VelocityArrowsDrawer drawer(system, ctx, 1.0f);

    // Enabled by default — should produce rays.
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.rays.size(), 0u);
    }

    // Disabled — no rays emitted.
    drawer.SetEnabled(false);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.rays.size(), 0u);
    }

    // Re-enabled — draws again.
    drawer.SetEnabled(true);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.rays.size(), 0u);
    }
}

// AC-15: each drawer emits the correct primitive type.
TEST(SteeringVisualDebugger_Identity, PrimitiveType)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    agent.position          = Dia::Maths::Vector2D(0.0f, 0.0f);
    agent.velocity          = Dia::Maths::Vector2D(2.0f, 0.0f);
    agent.maxSpeed          = 5.0f;
    agent.separationRadius  = 1.5f;
    agent.detectionBoxLength = 2.0f;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);

    StubDebugContext ctx;

    // VelocityArrowsDrawer — must call RequestDrawRay.
    {
        Dia::Steering::VelocityArrowsDrawer drawer(system, ctx, 1.0f);
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.rays.size(), 0u)
            << "VelocityArrowsDrawer must call RequestDrawRay";
    }

    // SeparationRadiusDrawer — must call RequestDraw (circle).
    {
        Dia::Steering::SeparationRadiusDrawer drawer(system, ctx);
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.circles.size(), 0u)
            << "SeparationRadiusDrawer must call RequestDraw(circle)";
    }

    // DetectionBoxDrawer — must call RequestDrawRect.
    {
        Dia::Steering::DetectionBoxDrawer drawer(system, ctx);
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_GT(draw.rects.size(), 0u)
            << "DetectionBoxDrawer must call RequestDrawRect";
    }
}

// AC-15: arrow length and circle radius both scale linearly with IDebugContext::GetDebugScale().
TEST(SteeringVisualDebugger_Identity, ScaleSensitivity)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    agent.position          = Dia::Maths::Vector2D(0.0f, 0.0f);
    agent.velocity          = Dia::Maths::Vector2D(2.0f, 0.0f);  // speed = 2
    agent.maxSpeed          = 5.0f;
    agent.separationRadius  = 1.5f;
    // No Seek pipeline — desired velocity stays zero, so exactly 1 ray is drawn.
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);

    StubDebugContext ctx;

    // Arrow length doubles when debug scale doubles.
    {
        Dia::Steering::VelocityArrowsDrawer drawer(system, ctx, 1.0f);

        ctx.scale = 1.0f;
        MockDraw draw1;
        drawer.Draw(draw1);
        ASSERT_EQ(draw1.rays.size(), 1u);
        const float L1 = draw1.rays[0].length;

        ctx.scale = 2.0f;
        MockDraw draw2;
        drawer.Draw(draw2);
        ASSERT_EQ(draw2.rays.size(), 1u);
        const float L2 = draw2.rays[0].length;

        EXPECT_NEAR(L2, L1 * 2.0f, 0.001f)
            << "Ray length must scale linearly with IDebugContext::GetDebugScale()";
    }

    // Circle radius doubles when debug scale doubles.
    {
        Dia::Steering::SeparationRadiusDrawer drawer(system, ctx);

        ctx.scale = 1.0f;
        MockDraw draw1;
        drawer.Draw(draw1);
        ASSERT_EQ(draw1.circles.size(), 1u);
        const float R1 = draw1.circles[0].radius;

        ctx.scale = 2.0f;
        MockDraw draw2;
        drawer.Draw(draw2);
        ASSERT_EQ(draw2.circles.size(), 1u);
        const float R2 = draw2.circles[0].radius;

        EXPECT_NEAR(R2, R1 * 2.0f, 0.001f)
            << "Circle radius must scale linearly with IDebugContext::GetDebugScale()";
    }
}

// AC-15: GetJSONState reports correct drawer names and default enabled states.
TEST(SteeringVisualDebugger_Identity, JSONRoundTrip)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringVisualDebugger domain(system);

    // Verify initial state.
    Json::Value s1;
    domain.GetJSONState(s1);
    ASSERT_TRUE(s1.isMember("drawers"));
    ASSERT_EQ(s1["drawers"].size(), 3u);
    EXPECT_STREQ(s1["drawers"][0u]["name"].asCString(), "VelocityArrows");
    EXPECT_TRUE(s1["drawers"][0u]["enabled"].asBool());
    EXPECT_STREQ(s1["drawers"][2u]["name"].asCString(), "DetectionBoxes");
    EXPECT_FALSE(s1["drawers"][2u]["enabled"].asBool());

    // Toggle VelocityArrows off and verify state is reflected.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("VelocityArrows"));

    Json::Value s2;
    domain.GetJSONState(s2);
    EXPECT_FALSE(s2["drawers"][0u]["enabled"].asBool())
        << "VelocityArrows must be disabled after toggle";
}

// AC-15: two toggles restore the original enabled state.
TEST(SteeringVisualDebugger_Identity, OnCommandRoundTrip)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringVisualDebugger domain(system);

    // First toggle — disable.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("VelocityArrows"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_FALSE(s["drawers"][0u]["enabled"].asBool());
    }

    // Second toggle — restore.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("VelocityArrows"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_TRUE(s["drawers"][0u]["enabled"].asBool());
    }
}

// ===========================================================================
// Suite: SteeringVisualDebugger_Arrows
// ===========================================================================

// VelocityArrowsDrawer draws exactly 2 rays per agent (current vel + desired vel)
// when both velocities have squared magnitude > 1e-8.
TEST(SteeringVisualDebugger_Arrows, TwoArrowsPerAgent)
{
    Dia::Steering::SteeringSystem system;
    Dia::Core::Containers::DynamicArrayC<
        std::pair<Dia::Steering::SteeringAgentId, Dia::Steering::SteeringPipeline>, 256> pipelines;

    for (int i = 0; i < 2; ++i)
    {
        Dia::Steering::SteeringAgent agent;
        agent.position = Dia::Maths::Vector2D(static_cast<float>(i) * 3.0f, 0.0f);
        agent.velocity = Dia::Maths::Vector2D(2.0f, 0.0f);
        agent.maxSpeed = 10.0f;

        const Dia::Core::StringCRC id(kAgentIds[i]);
        system.AddAgent(id, agent);

        Dia::Steering::SteeringPipeline pipeline;
        pipeline.AddContribution(0, 1.0f,
            Dia::Steering::Seek(agent, Dia::Maths::Vector2D(100.0f, 0.0f)), 1.0f);
        pipelines.Add(std::make_pair(id, pipeline));
    }
    system.Update(0.016f, pipelines);

    StubDebugContext ctx;
    Dia::Steering::VelocityArrowsDrawer drawer(system, ctx, 1.0f);
    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.rays.size(), 4u)
        << "2 rays per agent (current + desired), 2 agents = 4 total";
}

// Current and desired velocity arrows use distinct colours (kWarning vs kGoal).
TEST(SteeringVisualDebugger_Arrows, ColoursDiffer)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    agent.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    agent.velocity = Dia::Maths::Vector2D(2.0f, 0.0f);
    agent.maxSpeed = 10.0f;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);

    // Seek pipeline gives a non-zero desired velocity.
    Dia::Core::Containers::DynamicArrayC<
        std::pair<Dia::Steering::SteeringAgentId, Dia::Steering::SteeringPipeline>, 256> pipelines;
    Dia::Steering::SteeringPipeline pipeline;
    pipeline.AddContribution(0, 1.0f,
        Dia::Steering::Seek(agent, Dia::Maths::Vector2D(100.0f, 0.0f)), 1.0f);
    pipelines.Add(std::make_pair(Dia::Core::StringCRC(kAgentIds[0]), pipeline));
    system.Update(0.016f, pipelines);

    StubDebugContext ctx;
    Dia::Steering::VelocityArrowsDrawer drawer(system, ctx, 1.0f);
    MockDraw draw;
    drawer.Draw(draw);

    ASSERT_EQ(draw.rays.size(), 2u);
    EXPECT_NE(draw.rays[0].colour, draw.rays[1].colour)
        << "Current (kWarning) and desired (kGoal) arrows must have distinct colours";
    // Current velocity arrow = yellow (kWarning); drawn first inside VisitAgents.
    EXPECT_EQ(draw.rays[0].colour, Dia::Debug::DebugColourPalette::kWarning)
        << "Current velocity arrow must use kWarning (yellow)";
    // Desired velocity arrow = cyan (kGoal); drawn second.
    EXPECT_EQ(draw.rays[1].colour, Dia::Debug::DebugColourPalette::kGoal)
        << "Desired velocity arrow must use kGoal (cyan)";
}

// SetArrowScale() doubles ray length when scale goes from 1.0 to 2.0.
TEST(SteeringVisualDebugger_Arrows, ArrowScale_Command)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    agent.position = Dia::Maths::Vector2D(0.0f, 0.0f);
    agent.velocity = Dia::Maths::Vector2D(2.0f, 0.0f);
    agent.maxSpeed = 5.0f;
    // No Seek pipeline — only the current velocity arrow is drawn (1 ray).
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);

    StubDebugContext ctx;
    ctx.scale = 1.0f;
    Dia::Steering::VelocityArrowsDrawer drawer(system, ctx, 1.0f);

    MockDraw draw1;
    drawer.Draw(draw1);
    ASSERT_EQ(draw1.rays.size(), 1u);
    const float L1 = draw1.rays[0].length;

    drawer.SetArrowScale(2.0f);

    MockDraw draw2;
    drawer.Draw(draw2);
    ASSERT_EQ(draw2.rays.size(), 1u);
    const float L2 = draw2.rays[0].length;

    EXPECT_NEAR(L2, L1 * 2.0f, 0.001f)
        << "Ray length must double after SetArrowScale(2.0f)";
}

// ===========================================================================
// Suite: SteeringVisualDebugger_SeparationRadius
// ===========================================================================

// SeparationRadiusDrawer draws exactly one circle per agent that has
// separationRadius > 0.
TEST(SteeringVisualDebugger_SeparationRadius, OneCirclePerAgent)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent a;
    a.separationRadius = 1.5f;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), a);
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[1]), a);

    StubDebugContext ctx;
    Dia::Steering::SeparationRadiusDrawer drawer(system, ctx);
    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.circles.size(), 2u) << "one circle per agent";
}

// SetEnabled(false) suppresses the circle draw for SeparationRadiusDrawer.
TEST(SteeringVisualDebugger_SeparationRadius, DrawerGate_SeparationRadius)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    agent.separationRadius = 1.5f;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);

    StubDebugContext ctx;
    Dia::Steering::SeparationRadiusDrawer drawer(system, ctx);

    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.circles.size(), 1u);
    }

    drawer.SetEnabled(false);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.circles.size(), 0u);
    }
}

// OnCommand("toggle", {drawer:"SeparationRadius"}) toggles the enabled flag.
TEST(SteeringVisualDebugger_SeparationRadius, Toggle_SeparationRadius)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringVisualDebugger domain(system);

    // SeparationRadius starts enabled — toggle off.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("SeparationRadius"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_STREQ(s["drawers"][1u]["name"].asCString(), "SeparationRadius");
        EXPECT_FALSE(s["drawers"][1u]["enabled"].asBool());
    }

    // Toggle back on.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("SeparationRadius"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_TRUE(s["drawers"][1u]["enabled"].asBool());
    }
}

// ===========================================================================
// Suite: SteeringVisualDebugger_DetectionBoxes
// ===========================================================================

// DetectionBoxDrawer draws exactly one rect per agent that has
// detectionBoxLength > 0.
TEST(SteeringVisualDebugger_DetectionBoxes, OneBoxPerAgent)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent a;
    a.detectionBoxLength = 2.0f;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), a);
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[1]), a);

    StubDebugContext ctx;
    Dia::Steering::DetectionBoxDrawer drawer(system, ctx);
    MockDraw draw;
    drawer.Draw(draw);

    EXPECT_EQ(draw.rects.size(), 2u) << "one rect per agent";
}

// SetEnabled(false) suppresses the rect draw for DetectionBoxDrawer.
TEST(SteeringVisualDebugger_DetectionBoxes, DrawerGate_DetectionBoxes)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    agent.detectionBoxLength = 2.0f;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);

    StubDebugContext ctx;
    // IVisualDebugger default is enabled = true.
    Dia::Steering::DetectionBoxDrawer drawer(system, ctx);

    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.rects.size(), 1u);
    }

    drawer.SetEnabled(false);
    {
        MockDraw draw;
        drawer.Draw(draw);
        EXPECT_EQ(draw.rects.size(), 0u);
    }
}

// OnCommand("toggle", {drawer:"DetectionBoxes"}) flips the OFF default to ON.
TEST(SteeringVisualDebugger_DetectionBoxes, Toggle_DetectionBoxes)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringVisualDebugger domain(system);

    // DetectionBoxes is OFF by default (SD-002). One toggle must enable it.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("DetectionBoxes"));
    {
        Json::Value s;
        domain.GetJSONState(s);
        EXPECT_STREQ(s["drawers"][2u]["name"].asCString(), "DetectionBoxes");
        EXPECT_TRUE(s["drawers"][2u]["enabled"].asBool())
            << "DetectionBoxes must be ON after one toggle from the OFF default";
    }
}

// GetJSONState reports DetectionBoxes disabled on a freshly constructed domain.
TEST(SteeringVisualDebugger_DetectionBoxes, DetectionBoxes_OffByDefault)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringVisualDebugger domain(system);

    Json::Value s;
    domain.GetJSONState(s);

    ASSERT_EQ(s["drawers"].size(), 3u);
    EXPECT_STREQ(s["drawers"][2u]["name"].asCString(), "DetectionBoxes");
    EXPECT_FALSE(s["drawers"][2u]["enabled"].asBool())
        << "DetectionBoxes must be disabled by default (SD-002)";
}

// ===========================================================================
// Suite: SteeringVisualDebugger_Stats
// ===========================================================================

// GetJSONState emits stats.agentCount matching the number of registered agents.
TEST(SteeringVisualDebugger_Stats, AgentCount_Accurate)
{
    Dia::Steering::SteeringSystem system;
    Dia::Steering::SteeringAgent agent;
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[0]), agent);
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[1]), agent);
    system.AddAgent(Dia::Core::StringCRC(kAgentIds[2]), agent);

    Dia::Steering::SteeringVisualDebugger domain(system);
    Json::Value s;
    domain.GetJSONState(s);

    EXPECT_EQ(s["stats"]["agentCount"].asInt(), 3);
}

// An empty system produces zero primitives from all three drawers and does not crash.
TEST(SteeringVisualDebugger_Stats, NoAgents_NoOutput_NoAssert)
{
    Dia::Steering::SteeringSystem system; // empty

    StubDebugContext ctx;

    {
        Dia::Steering::VelocityArrowsDrawer drawer(system, ctx, 1.0f);
        MockDraw draw;
        EXPECT_NO_FATAL_FAILURE(drawer.Draw(draw));
        EXPECT_EQ(draw.rays.size(), 0u);
    }
    {
        Dia::Steering::SeparationRadiusDrawer drawer(system, ctx);
        MockDraw draw;
        EXPECT_NO_FATAL_FAILURE(drawer.Draw(draw));
        EXPECT_EQ(draw.circles.size(), 0u);
    }
    {
        Dia::Steering::DetectionBoxDrawer drawer(system, ctx);
        MockDraw draw;
        EXPECT_NO_FATAL_FAILURE(drawer.Draw(draw));
        EXPECT_EQ(draw.rects.size(), 0u);
    }
}

#endif // DIA_DEBUG
