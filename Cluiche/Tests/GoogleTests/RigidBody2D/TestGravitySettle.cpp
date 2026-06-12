#include <gtest/gtest.h>
#include <cmath>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaMaths/Vector/Vector2D.h>

// ===========================================================================
// Gravity-settle integration tests.
//
// These reproduce the RigidBody2DTestStage scenario at the engine level:
// dynamic circles fall under gravity, hit a static ground, and must come to
// rest. This is the path the in-app stage exercises and the one that was
// never covered by an existing GoogleTest.
//
// We test it twice on purpose:
//   * WithBroadPhase  — the production-intended acceleration path
//   * WithoutBroadPhase — the brute-force fallback (matches the app today,
//                          which never assigns a broadphase)
// If the math is correct, both must settle.
// ===========================================================================

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

namespace {

using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

static constexpr float kDt = 1.0f / 60.0f;

struct SettleScene
{
    static constexpr int kCircleCount = 6;

    Dia::Geometry2D::Transform    groundT;
    Dia::Geometry2D::ConvexPolygon groundShape;
    Dia::Geometry2D::Transform    circleT[kCircleCount];
    Dia::Geometry2D::Circle       circleShape[kCircleCount];

    Grid*         grid  = nullptr;
    PhysicsWorld* world = nullptr;
    RigidBody2D*  ground = nullptr;
    RigidBody2D*  circles[kCircleCount] = {};

    explicit SettleScene(bool useBroadPhase)
    {
        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, -9.81f);
        wd.fixedTimestep = kDt;
        wd.maxSubSteps   = 8;

        if (useBroadPhase)
        {
            Grid::Def gd;
            gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-50.0f, -50.0f),
                                                     Vector2D(50.0f, 50.0f));
            gd.cellSize = 5.0f;
            grid = new Grid(gd);
            wd.broadPhase = grid;
        }

        world = new PhysicsWorld(wd);

        // Static ground: a wide, flat box (ConvexPolygon) whose top sits at y = 0.
        // A flat surface is what bodies actually settle on — a curved ground would
        // let them slide off the sides and fall forever (correct physics, useless test).
        groundT.SetWorldPosition(Vector2D(0.0f, 0.0f));
        const Vector2D groundVerts[4] = {
            Vector2D(-20.0f, -2.0f), Vector2D(20.0f, -2.0f),
            Vector2D( 20.0f,  0.0f), Vector2D(-20.0f, 0.0f)
        };
        groundShape = Dia::Geometry2D::ConvexPolygon(groundVerts, 4);
        RigidBodyDef gdef;
        gdef.id          = Dia::Core::StringCRC("ground");
        gdef.transform   = &groundT;
        gdef.polyShape   = &groundShape;
        gdef.type        = BodyType::kStatic;
        gdef.mass        = 0.0f;
        gdef.restitution = 0.1f;
        gdef.friction    = 0.6f;
        ground = world->AddRigidBody(gdef);

        // Dynamic circles dropped a short distance above the ground in a row.
        for (int i = 0; i < kCircleCount; ++i)
        {
            float x = -5.0f + static_cast<float>(i) * 2.0f;
            float y = 2.0f + static_cast<float>(i) * 0.6f;
            circleT[i].SetWorldPosition(Vector2D(x, y));
            circleShape[i] = Dia::Geometry2D::Circle(0.5f, Vector2D::Zero());

            RigidBodyDef cdef;
            cdef.id            = Dia::Core::StringCRC("circle");
            cdef.transform     = &circleT[i];
            cdef.circleShape   = &circleShape[i];
            cdef.type          = BodyType::kDynamic;
            cdef.mass          = 1.0f;
            cdef.restitution   = 0.1f;
            cdef.friction      = 0.6f;
            cdef.linearDamping = 0.05f;
            cdef.allowSleeping = true;
            circles[i] = world->AddRigidBody(cdef);
        }
    }

    ~SettleScene() { delete world; delete grid; }

    bool AllAsleep() const
    {
        for (int i = 0; i < kCircleCount; ++i)
            if (circles[i] && circles[i]->IsAwake())
                return false;
        return true;
    }

    float MaxSpeed() const
    {
        float m = 0.0f;
        for (int i = 0; i < kCircleCount; ++i)
        {
            const Vector2D& v = circles[i]->GetVelocity();
            float s = std::sqrt(v.x * v.x + v.y * v.y);
            if (s > m) m = s;
        }
        return m;
    }

    float LowestY() const
    {
        float lo = 1e30f;
        for (int i = 0; i < kCircleCount; ++i)
        {
            float y = circles[i]->GetTransform()->GetWorldPosition().y;
            if (y < lo) lo = y;
        }
        return lo;
    }

    int StepUntilSettled(int maxSteps)
    {
        for (int s = 0; s < maxSteps; ++s)
        {
            world->Update(kDt);
            if (AllAsleep())
                return s;
        }
        return -1;
    }
};

} // namespace

TEST(RigidBody2D_GravitySettle, WithBroadPhase_CirclesComeToRest)
{
    SettleScene scene(/*useBroadPhase=*/true);

    int settledStep = scene.StepUntilSettled(1800);  // 30 sim-seconds budget

    EXPECT_GE(settledStep, 0)
        << "Circles never settled. maxSpeed=" << scene.MaxSpeed()
        << " lowestY=" << scene.LowestY();
    EXPECT_GT(scene.LowestY(), -5.0f)
        << "Circles fell through the ground — collision not resolving";
}

TEST(RigidBody2D_GravitySettle, WithoutBroadPhase_CirclesComeToRest)
{
    SettleScene scene(/*useBroadPhase=*/false);

    int settledStep = scene.StepUntilSettled(1800);

    EXPECT_GE(settledStep, 0)
        << "Circles never settled (brute-force path). maxSpeed=" << scene.MaxSpeed()
        << " lowestY=" << scene.LowestY();
    EXPECT_GT(scene.LowestY(), -5.0f)
        << "Circles fell through the ground (brute-force path)";
}

TEST(RigidBody2D_GravitySettle, WithoutBroadPhase_DetectsContacts)
{
    SettleScene scene(/*useBroadPhase=*/false);

    bool sawContact = false;
    for (int s = 0; s < 600 && !sawContact; ++s)
    {
        scene.world->Update(kDt);
        if (scene.world->GetLastContacts().Size() > 0)
            sawContact = true;
    }

    EXPECT_TRUE(sawContact)
        << "Brute-force DetectCollisions produced no contacts in 10s of falling";
}

// ---------------------------------------------------------------------------
// Minimal diagnostic: ONE circle dropped straight down onto a static circle
// directly below it, small drop, low gravity — no sliding, no tunneling.
// If this can't stop a single ball, collision response is broken in the loop.
// ---------------------------------------------------------------------------
TEST(RigidBody2D_GravitySettle, SingleCircle_DropsAndStops)
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;
    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-20.0f, -20.0f), Vector2D(20.0f, 20.0f));
    gd.cellSize = 2.0f;
    Grid grid(gd);

    WorldDef wd;
    wd.gravity       = Vector2D(0.0f, -9.81f);
    wd.fixedTimestep = kDt;
    wd.maxSubSteps   = 8;
    wd.broadPhase    = &grid;
    PhysicsWorld world(wd);

    // Static ground circle centred at origin, radius 2 → top at y = 2.
    Dia::Geometry2D::Transform groundT;
    groundT.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle groundShape(2.0f, Vector2D::Zero());
    RigidBodyDef gdef;
    gdef.id = Dia::Core::StringCRC("g"); gdef.transform = &groundT;
    gdef.circleShape = &groundShape; gdef.type = BodyType::kStatic;
    gdef.mass = 0.0f; gdef.restitution = 0.0f; gdef.friction = 0.5f;
    world.AddRigidBody(gdef);

    // Dynamic ball directly above, radius 0.5, dropped from y = 4 (top at 2.5,
    // resting contact at y = 2.5). Short fall, stays slow.
    Dia::Geometry2D::Transform ballT;
    ballT.SetWorldPosition(Vector2D(0.0f, 4.0f));
    Dia::Geometry2D::Circle ballShape(0.5f, Vector2D::Zero());
    RigidBodyDef bdef;
    bdef.id = Dia::Core::StringCRC("b"); bdef.transform = &ballT;
    bdef.circleShape = &ballShape; bdef.type = BodyType::kDynamic;
    bdef.mass = 1.0f; bdef.restitution = 0.0f; bdef.friction = 0.5f;
    bdef.allowSleeping = true;
    RigidBody2D* ball = world.AddRigidBody(bdef);

    float minY = 1e30f, maxcontactDepth = 0.0f;
    int contactSteps = 0;
    for (int s = 0; s < 600; ++s)
    {
        world.Update(kDt);
        float y = ball->GetTransform()->GetWorldPosition().y;
        if (y < minY) minY = y;
        if (world.GetLastContacts().Size() > 0)
        {
            ++contactSteps;
            float d = world.GetLastContacts()[0].depth;
            if (d > maxcontactDepth) maxcontactDepth = d;
        }
    }

    float finalY = ball->GetTransform()->GetWorldPosition().y;
    const Vector2D& v = ball->GetVelocity();
    float finalSpeed = std::sqrt(v.x * v.x + v.y * v.y);

    // Expected resting centre ≈ y = 2.5 (ground top 2.0 + ball radius 0.5).
    EXPECT_GT(finalY, 2.0f)
        << "Ball sank into/through ground. finalY=" << finalY
        << " minY=" << minY << " contactSteps=" << contactSteps;
    EXPECT_LT(finalSpeed, 0.5f)
        << "Ball never came to rest. finalSpeed=" << finalSpeed
        << " finalY=" << finalY;
}

// ---------------------------------------------------------------------------
// Same minimal drop, but ground is a flat ConvexPolygon instead of a circle.
// Isolates the circle-vs-polygon collision path (which DetectCollisions routes
// through the AARect fallback). If this fails while the circle-ground version
// passes, circle-vs-poly is the broken path.
// ---------------------------------------------------------------------------
TEST(RigidBody2D_GravitySettle, SingleCircle_OnPolyGround_Stops)
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;
    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-30.0f, -30.0f), Vector2D(30.0f, 30.0f));
    gd.cellSize = 3.0f;
    Grid grid(gd);

    WorldDef wd;
    wd.gravity       = Vector2D(0.0f, -9.81f);
    wd.fixedTimestep = kDt;
    wd.maxSubSteps   = 8;
    wd.broadPhase    = &grid;
    PhysicsWorld world(wd);

    // Flat box ground, top edge at y = 0.
    Dia::Geometry2D::Transform groundT;
    groundT.SetWorldPosition(Vector2D(0.0f, 0.0f));
    const Vector2D gv[4] = {
        Vector2D(-10.0f, -2.0f), Vector2D(10.0f, -2.0f),
        Vector2D( 10.0f,  0.0f), Vector2D(-10.0f, 0.0f)
    };
    Dia::Geometry2D::ConvexPolygon groundShape(gv, 4);
    RigidBodyDef gdef;
    gdef.id = Dia::Core::StringCRC("g"); gdef.transform = &groundT;
    gdef.polyShape = &groundShape; gdef.type = BodyType::kStatic;
    gdef.mass = 0.0f; gdef.restitution = 0.0f; gdef.friction = 0.5f;
    world.AddRigidBody(gdef);

    Dia::Geometry2D::Transform ballT;
    ballT.SetWorldPosition(Vector2D(0.0f, 2.0f));
    Dia::Geometry2D::Circle ballShape(0.5f, Vector2D::Zero());
    RigidBodyDef bdef;
    bdef.id = Dia::Core::StringCRC("b"); bdef.transform = &ballT;
    bdef.circleShape = &ballShape; bdef.type = BodyType::kDynamic;
    bdef.mass = 1.0f; bdef.restitution = 0.0f; bdef.friction = 0.5f;
    bdef.allowSleeping = true;
    RigidBody2D* ball = world.AddRigidBody(bdef);

    float minY = 1e30f;
    for (int s = 0; s < 600; ++s)
    {
        world.Update(kDt);
        float y = ball->GetTransform()->GetWorldPosition().y;
        if (y < minY) minY = y;
    }

    float finalY = ball->GetTransform()->GetWorldPosition().y;
    const Vector2D& v = ball->GetVelocity();
    float finalSpeed = std::sqrt(v.x * v.x + v.y * v.y);

    EXPECT_GT(finalY, 0.0f)
        << "Ball sank through poly ground. finalY=" << finalY << " minY=" << minY;
    EXPECT_LT(finalSpeed, 0.5f)
        << "Ball never came to rest on poly ground. finalSpeed=" << finalSpeed;
}

// ---------------------------------------------------------------------------
// Same single circle on the SAME poly ground, but dropped from higher up so it
// penetrates deeper in one step. Hypothesis: circle-vs-poly falls through to
// AARectVsAARect (box treatment), which flips to a horizontal normal on deep
// penetration and ejects the ball sideways. If this fails while the gentle
// drop passes, the missing circle-vs-poly narrow phase is the root cause.
// ---------------------------------------------------------------------------
TEST(RigidBody2D_GravitySettle, SingleCircle_OnPolyGround_HighDrop_Stops)
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;
    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-30.0f, -30.0f), Vector2D(30.0f, 30.0f));
    gd.cellSize = 3.0f;
    Grid grid(gd);

    WorldDef wd;
    wd.gravity       = Vector2D(0.0f, -9.81f);
    wd.fixedTimestep = kDt;
    wd.maxSubSteps   = 8;
    wd.broadPhase    = &grid;
    PhysicsWorld world(wd);

    Dia::Geometry2D::Transform groundT;
    groundT.SetWorldPosition(Vector2D(0.0f, 0.0f));
    const Vector2D gv[4] = {
        Vector2D(-10.0f, -2.0f), Vector2D(10.0f, -2.0f),
        Vector2D( 10.0f,  0.0f), Vector2D(-10.0f, 0.0f)
    };
    Dia::Geometry2D::ConvexPolygon groundShape(gv, 4);
    RigidBodyDef gdef;
    gdef.id = Dia::Core::StringCRC("g"); gdef.transform = &groundT;
    gdef.polyShape = &groundShape; gdef.type = BodyType::kStatic;
    gdef.mass = 0.0f; gdef.restitution = 0.0f; gdef.friction = 0.5f;
    world.AddRigidBody(gdef);

    Dia::Geometry2D::Transform ballT;
    ballT.SetWorldPosition(Vector2D(0.0f, 12.0f));   // high drop
    Dia::Geometry2D::Circle ballShape(0.5f, Vector2D::Zero());
    RigidBodyDef bdef;
    bdef.id = Dia::Core::StringCRC("b"); bdef.transform = &ballT;
    bdef.circleShape = &ballShape; bdef.type = BodyType::kDynamic;
    bdef.mass = 1.0f; bdef.restitution = 0.0f; bdef.friction = 0.5f;
    bdef.allowSleeping = true;
    RigidBody2D* ball = world.AddRigidBody(bdef);

    float minY = 1e30f, maxAbsX = 0.0f;
    for (int s = 0; s < 600; ++s)
    {
        world.Update(kDt);
        Vector2D p = ball->GetTransform()->GetWorldPosition();
        if (p.y < minY) minY = p.y;
        if (std::abs(p.x) > maxAbsX) maxAbsX = std::abs(p.x);
    }

    float finalY = ball->GetTransform()->GetWorldPosition().y;
    EXPECT_GT(finalY, 0.0f)
        << "High-dropped ball sank through poly ground. finalY=" << finalY
        << " minY=" << minY << " maxAbsX=" << maxAbsX
        << " (maxAbsX large => ejected sideways by box-normal bug)";
}
