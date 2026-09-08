#include <gtest/gtest.h>
#include <cmath>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/World/WorldDef.h>
#include <DiaRigidBody2D/Detection/DetectCollisions.h>
#include <DiaRigidBody2D/Detection/Contact.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

// ===========================================================================
// Pinpoints the circle-vs-ConvexPolygon contact-point defect.
//
// A circle resting on a flat polygon ground, offset in X, must produce a
// contact point directly beneath the circle (point.x ~= circle.x). The current
// NarrowPhase has no circle-vs-poly branch, so it falls through to the
// AARectVsAARect fallback, which reports the ground box's centre-top face
// (point.x == ground centre) for ALL circles regardless of their X position.
// ===========================================================================

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

TEST(RigidBody2D_CircleVsPolyContact, ContactPointIsBeneathCircle_NotGroundCentre)
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;
    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-30.0f, -30.0f), Vector2D(30.0f, 30.0f));
    gd.cellSize = 3.0f;
    Grid grid(gd);

    // Flat box ground centred at origin, top edge y = 0.
    Dia::Geometry2D::Transform groundT;
    groundT.SetWorldPosition(Vector2D(0.0f, 0.0f));
    const Vector2D gv[4] = {
        Vector2D(-20.0f, -2.0f), Vector2D(20.0f, -2.0f),
        Vector2D( 20.0f,  0.0f), Vector2D(-20.0f, 0.0f)
    };
    Dia::Geometry2D::ConvexPolygon groundShape(gv, 4);
    RigidBodyDef gdef;
    gdef.id = Dia::Core::StringCRC("g"); gdef.transform = &groundT;
    gdef.polyShape = &groundShape; gdef.type = BodyType::kStatic; gdef.mass = 0.0f;

    // Circle resting on the ground far from the centre: centre at x = 8, just
    // touching the top edge (y slightly < radius so it overlaps).
    const float kCircleX = 8.0f;
    Dia::Geometry2D::Transform circleT;
    circleT.SetWorldPosition(Vector2D(kCircleX, 0.4f));   // radius 0.5 → 0.1 penetration
    Dia::Geometry2D::Circle circleShape(0.5f, Vector2D::Zero());
    RigidBodyDef cdef;
    cdef.id = Dia::Core::StringCRC("c"); cdef.transform = &circleT;
    cdef.circleShape = &circleShape; cdef.type = BodyType::kDynamic; cdef.mass = 1.0f;

    // Build the bodies via a world so AABBs register in the grid.
    WorldDef wd;
    wd.gravity = Vector2D::Zero();
    wd.broadPhase = &grid;
    PhysicsWorld world(wd);
    world.AddRigidBody(gdef);
    world.AddRigidBody(cdef);

    // One step runs DetectCollisions; inspect the resulting contact.
    world.Update(1.0f / 60.0f);

    const unsigned int contactCount = world.GetLastContacts().Size();
    ASSERT_GE(contactCount, 1u) << "No contact generated for resting circle";

    const Contact& c = world.GetLastContacts()[0];
    // The contact should sit under the circle (x ~= 8), NOT at the ground
    // centre (x == 0). The AABB fallback reports x == 0.
    EXPECT_NEAR(c.point.x, kCircleX, 1.0f)
        << "Contact point is at x=" << c.point.x
        << " (expected ~" << kCircleX << "). Circle-vs-poly is using the "
        << "AABB-vs-AABB fallback, which reports the ground's centre face.";

    // The normal should be vertical (ground pushing circle up).
    EXPECT_NEAR(c.normal.x, 0.0f, 0.1f) << "Contact normal should be vertical";
    EXPECT_GT(std::abs(c.normal.y), 0.9f) << "Contact normal should be vertical";
}
