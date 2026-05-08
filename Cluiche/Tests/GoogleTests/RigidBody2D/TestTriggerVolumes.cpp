#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Triggers/TriggerVolume2D.h>
#include <DiaRigidBody2D/Triggers/TriggerEvent.h>
#include <DiaRigidBody2D/Detection/DetectCollisions.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Architecture/Observer.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

// ---------------------------------------------------------------------------
// Fixture — zero gravity, broadphase-backed world
// ---------------------------------------------------------------------------

struct TriggerFixture {
    Grid*         grid  = nullptr;
    PhysicsWorld* world = nullptr;

    TriggerFixture()
    {
        Grid::Def gd;
        gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
        gd.cellSize    = 10.0f;
        grid = new Grid(gd);

        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, 0.0f);
        wd.fixedTimestep = 1.0f / 60.0f;
        wd.maxSubSteps   = 1;
        wd.broadPhase    = grid;
        world = new PhysicsWorld(wd);
    }

    ~TriggerFixture() { delete world; delete grid; }
};

// ---------------------------------------------------------------------------
// Test 1 — TriggerVolume construction
// ---------------------------------------------------------------------------

TEST(RigidBody2D_TriggerVolumes, Construction_PropertiesMatchDef)
{
    Dia::Geometry2D::Transform t;
    t.SetWorldPosition(Vector2D(5.0f, 5.0f));
    Dia::Geometry2D::Circle circ(3.0f, Vector2D::Zero());

    TriggerVolumeDef def;
    def.transform   = &t;
    def.circleShape = &circ;
    def.layer       = Layers::kPlayer;
    def.mask        = Layers::kEnemy;

    TriggerVolume2D trigger(def);

    EXPECT_EQ(trigger.GetShapeKind(), ShapeKind::kCircle);
    EXPECT_NE(trigger.GetCircleShape(), nullptr);
    EXPECT_EQ(trigger.GetPolyShape(), nullptr);
    EXPECT_NEAR(trigger.GetCircleShape()->GetRadius(), 3.0f, 1e-5f);
    EXPECT_EQ(trigger.GetLayer(), Layers::kPlayer);
    EXPECT_EQ(trigger.GetMask(), Layers::kEnemy);
    EXPECT_NEAR(trigger.GetTransform()->GetWorldPosition().x, 5.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test 2 — Trigger enters when body overlaps
// ---------------------------------------------------------------------------

TEST(RigidBody2D_TriggerVolumes, BodyEntersTrigger_EnterEventFired)
{
    TriggerFixture f;

    Dia::Geometry2D::Transform tTrig;
    tTrig.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle trigCirc(5.0f, Vector2D::Zero());

    TriggerVolumeDef trigDef;
    trigDef.transform   = &tTrig;
    trigDef.circleShape = &trigCirc;
    f.world->AddTriggerVolume(trigDef);

    Dia::Geometry2D::Transform tBody;
    tBody.SetWorldPosition(Vector2D(3.0f, 0.0f));
    Dia::Geometry2D::Circle bodyCirc(1.0f, Vector2D::Zero());
    PointBodyDef bodyDef;
    bodyDef.transform   = &tBody;
    bodyDef.circleShape = &bodyCirc;
    bodyDef.type        = BodyType::kStatic;
    f.world->AddPointBody(bodyDef);

    f.world->Update(1.0f / 60.0f);

    const auto& events = f.world->GetLastTriggerEvents();
    ASSERT_GE(events.Size(), 1u);
    EXPECT_EQ(events[0].type, TriggerEventType::kEnter);
    EXPECT_NE(events[0].trigger, nullptr);
    EXPECT_NE(events[0].body, nullptr);
}

// ---------------------------------------------------------------------------
// Test 3 — Continued overlap emits Stay
// ---------------------------------------------------------------------------

TEST(RigidBody2D_TriggerVolumes, ContinuedOverlap_StayEventFired)
{
    TriggerFixture f;

    Dia::Geometry2D::Transform tTrig;
    tTrig.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle trigCirc(5.0f, Vector2D::Zero());

    TriggerVolumeDef trigDef;
    trigDef.transform   = &tTrig;
    trigDef.circleShape = &trigCirc;
    f.world->AddTriggerVolume(trigDef);

    Dia::Geometry2D::Transform tBody;
    tBody.SetWorldPosition(Vector2D(3.0f, 0.0f));
    Dia::Geometry2D::Circle bodyCirc(1.0f, Vector2D::Zero());
    PointBodyDef bodyDef;
    bodyDef.transform   = &tBody;
    bodyDef.circleShape = &bodyCirc;
    bodyDef.type        = BodyType::kStatic;
    f.world->AddPointBody(bodyDef);

    f.world->Update(1.0f / 60.0f);  // Enter
    f.world->Update(1.0f / 60.0f);  // Stay

    const auto& events = f.world->GetLastTriggerEvents();
    bool hasStay = false;
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        if (events[i].type == TriggerEventType::kStay)
            hasStay = true;
    }
    EXPECT_TRUE(hasStay);
}

// ---------------------------------------------------------------------------
// Test 4 — Body outside trigger produces no events
// ---------------------------------------------------------------------------

TEST(RigidBody2D_TriggerVolumes, BodyOutsideTrigger_NoEvents)
{
    TriggerFixture f;

    Dia::Geometry2D::Transform tTrig;
    tTrig.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle trigCirc(2.0f, Vector2D::Zero());

    TriggerVolumeDef trigDef;
    trigDef.transform   = &tTrig;
    trigDef.circleShape = &trigCirc;
    f.world->AddTriggerVolume(trigDef);

    Dia::Geometry2D::Transform tBody;
    tBody.SetWorldPosition(Vector2D(50.0f, 0.0f));
    Dia::Geometry2D::Circle bodyCirc(1.0f, Vector2D::Zero());
    PointBodyDef bodyDef;
    bodyDef.transform   = &tBody;
    bodyDef.circleShape = &bodyCirc;
    bodyDef.type        = BodyType::kStatic;
    f.world->AddPointBody(bodyDef);

    f.world->Update(1.0f / 60.0f);

    const auto& events = f.world->GetLastTriggerEvents();
    EXPECT_EQ(events.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 5 — Trigger does not produce physics contacts
// ---------------------------------------------------------------------------

TEST(RigidBody2D_TriggerVolumes, TriggerDoesNotProduceContacts)
{
    TriggerFixture f;

    Dia::Geometry2D::Transform tTrig;
    tTrig.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle trigCirc(5.0f, Vector2D::Zero());

    TriggerVolumeDef trigDef;
    trigDef.transform   = &tTrig;
    trigDef.circleShape = &trigCirc;
    f.world->AddTriggerVolume(trigDef);

    Dia::Geometry2D::Transform tBody;
    tBody.SetWorldPosition(Vector2D(3.0f, 0.0f));
    Dia::Geometry2D::Circle bodyCirc(1.0f, Vector2D::Zero());
    PointBodyDef bodyDef;
    bodyDef.transform   = &tBody;
    bodyDef.circleShape = &bodyCirc;
    bodyDef.type        = BodyType::kStatic;
    f.world->AddPointBody(bodyDef);

    f.world->Update(1.0f / 60.0f);

    EXPECT_EQ(f.world->GetLastContacts().Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 6 — Layer filtering applies to triggers
// ---------------------------------------------------------------------------

TEST(RigidBody2D_TriggerVolumes, LayerFiltering_FilteredPairNoEvent)
{
    TriggerFixture f;

    Dia::Geometry2D::Transform tTrig;
    tTrig.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle trigCirc(5.0f, Vector2D::Zero());

    TriggerVolumeDef trigDef;
    trigDef.transform   = &tTrig;
    trigDef.circleShape = &trigCirc;
    trigDef.layer       = Layers::kPlayer;
    trigDef.mask        = Layers::kEnemy;
    f.world->AddTriggerVolume(trigDef);

    Dia::Geometry2D::Transform tBody;
    tBody.SetWorldPosition(Vector2D(3.0f, 0.0f));
    Dia::Geometry2D::Circle bodyCirc(1.0f, Vector2D::Zero());
    PointBodyDef bodyDef;
    bodyDef.transform   = &tBody;
    bodyDef.circleShape = &bodyCirc;
    bodyDef.type        = BodyType::kStatic;
    bodyDef.layer       = Layers::kPlayer;
    bodyDef.mask        = Layers::kAll;
    f.world->AddPointBody(bodyDef);

    f.world->Update(1.0f / 60.0f);

    const auto& events = f.world->GetLastTriggerEvents();
    EXPECT_EQ(events.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 7 — Add and remove trigger volume, count is correct
// ---------------------------------------------------------------------------

TEST(RigidBody2D_TriggerVolumes, AddRemove_CountCorrect)
{
    TriggerFixture f;

    Dia::Geometry2D::Transform t;
    t.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle circ(1.0f, Vector2D::Zero());

    TriggerVolumeDef def;
    def.transform   = &t;
    def.circleShape = &circ;

    EXPECT_EQ(f.world->GetTriggerVolumeCount(), 0);

    TriggerVolume2D* trigger = f.world->AddTriggerVolume(def);
    EXPECT_EQ(f.world->GetTriggerVolumeCount(), 1);

    f.world->RemoveTriggerVolume(trigger);
    EXPECT_EQ(f.world->GetTriggerVolumeCount(), 0);
}

// ---------------------------------------------------------------------------
// Test 8 — Observer is notified on trigger events
// ---------------------------------------------------------------------------

class TriggerObserver : public Dia::Core::Observer {
public:
    int count = 0;
    void ObserverNotification(const Dia::Core::ObserverSubject*, int) override { ++count; }
};

TEST(RigidBody2D_TriggerVolumes, Observer_NotifiedOnEvents)
{
    TriggerFixture f;

    TriggerObserver obs;
    f.world->GetTriggerEvents().AttachToObserver(&obs);

    Dia::Geometry2D::Transform tTrig;
    tTrig.SetWorldPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle trigCirc(5.0f, Vector2D::Zero());

    TriggerVolumeDef trigDef;
    trigDef.transform   = &tTrig;
    trigDef.circleShape = &trigCirc;
    f.world->AddTriggerVolume(trigDef);

    Dia::Geometry2D::Transform tBody;
    tBody.SetWorldPosition(Vector2D(3.0f, 0.0f));
    Dia::Geometry2D::Circle bodyCirc(1.0f, Vector2D::Zero());
    PointBodyDef bodyDef;
    bodyDef.transform   = &tBody;
    bodyDef.circleShape = &bodyCirc;
    bodyDef.type        = BodyType::kStatic;
    f.world->AddPointBody(bodyDef);

    f.world->Update(1.0f / 60.0f);

    EXPECT_GT(obs.count, 0);
}
