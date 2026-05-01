#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

static WorldDef MakeWorldDef()
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, 0.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 1;
    def.solverIterations = 1;
    def.rigidBodyWorld = nullptr;
    return def;
}

static RopeDef MakeTestRopeDef()
{
    RopeDef def;
    def.id = Dia::Core::StringCRC("GeoRope");
    def.startPoint = Vector2D(0.0f, 0.0f);
    def.endPoint = Vector2D(1.0f, 0.0f);
    def.particleCount = 2;
    def.mass = 1.0f;
    def.stiffness = 1.0f;
    def.particleRadius = 0.1f;
    def.maxStretch = 0.0f;
    return def;
}

static ClothDef MakeTestClothDef()
{
    ClothDef def;
    def.id = Dia::Core::StringCRC("GeoCloth");
    def.origin = Vector2D(0.0f, 0.0f);
    def.width = 2.0f;
    def.height = 2.0f;
    def.resX = 3;
    def.resY = 3;
    def.mass = 1.0f;
    def.structuralStiffness = 1.0f;
    def.shearStiffness = 0.5f;
    def.bendStiffness = 0.1f;
    def.particleRadius = 0.1f;
    def.maxStretch = 0.0f;
    def.pinTopRow = false;
    return def;
}

TEST(SoftBody2D_GeometryCollision, ParticleVsAARect_PushedOut)
{
    Dia::Geometry2D::AARect rect(Vector2D(-1.0f, -1.0f), Vector2D(1.0f, 1.0f));

    WorldDef wdef = MakeWorldDef();
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&rect);

    RopeDef rdef = MakeTestRopeDef();
    rdef.startPoint = Vector2D(0.95f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    Rope* rope = world.AddRope(rdef);

    world.Update(wdef.fixedTimestep);

    float distFromEdge = rope->GetParticle(0).position.x - 1.0f;
    EXPECT_GE(distFromEdge, -0.01f);

    world.RemoveStaticShape(&rect);
}

TEST(SoftBody2D_GeometryCollision, ParticleVsCircle_PushedOut)
{
    Dia::Geometry2D::Circle circle(1.0f, Vector2D(0.0f, 0.0f));

    WorldDef wdef = MakeWorldDef();
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&circle);

    RopeDef rdef = MakeTestRopeDef();
    rdef.startPoint = Vector2D(0.85f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    Rope* rope = world.AddRope(rdef);

    world.Update(wdef.fixedTimestep);

    Vector2D pos = rope->GetParticle(0).position;
    float distFromCenter = pos.Magnitude();
    EXPECT_GE(distFromCenter, 1.0f + rope->GetParticle(0).radius - 0.01f);

    world.RemoveStaticShape(&circle);
}

TEST(SoftBody2D_GeometryCollision, ParticleVsLine_PushedOut)
{
    Dia::Geometry2D::Line line(Vector2D(-5.0f, 0.0f), Vector2D(5.0f, 0.0f));

    WorldDef wdef = MakeWorldDef();
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&line);

    RopeDef rdef = MakeTestRopeDef();
    rdef.startPoint = Vector2D(0.0f, 0.05f);
    rdef.endPoint = Vector2D(1.0f, 2.0f);
    rdef.particleRadius = 0.1f;
    Rope* rope = world.AddRope(rdef);

    world.Update(wdef.fixedTimestep);

    EXPECT_GE(rope->GetParticle(0).position.y, rope->GetParticle(0).radius - 0.01f);

    world.RemoveStaticShape(&line);
}

TEST(SoftBody2D_GeometryCollision, NoStaticShapes_NoCrash)
{
    WorldDef wdef = MakeWorldDef();
    wdef.gravity = Vector2D(0.0f, -9.81f);
    SoftBodyWorld world(wdef);
    world.AddRope(MakeTestRopeDef());

    world.Update(wdef.fixedTimestep);
    SUCCEED();
}

TEST(SoftBody2D_GeometryCollision, PinnedParticle_StillResolved)
{
    Dia::Geometry2D::AARect rect(Vector2D(-1.0f, -1.0f), Vector2D(1.0f, 1.0f));

    WorldDef wdef = MakeWorldDef();
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&rect);

    RopeDef rdef = MakeTestRopeDef();
    rdef.startPoint = Vector2D(0.5f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    world.Update(wdef.fixedTimestep);
    SUCCEED();

    world.RemoveStaticShape(&rect);
}

TEST(SoftBody2D_GeometryCollision, RemoveStaticShape_NoCollisionAfter)
{
    Dia::Geometry2D::AARect rect(Vector2D(-1.0f, -1.0f), Vector2D(1.0f, 1.0f));

    WorldDef wdef = MakeWorldDef();
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&rect);
    world.RemoveStaticShape(&rect);

    RopeDef rdef = MakeTestRopeDef();
    rdef.startPoint = Vector2D(0.5f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    world.AddRope(rdef);

    world.Update(wdef.fixedTimestep);
    SUCCEED();
}

TEST(SoftBody2D_GeometryCollision, ClothVsAARect_PushedOut)
{
    Dia::Geometry2D::AARect floor(Vector2D(-5.0f, -2.0f), Vector2D(5.0f, -1.0f));

    WorldDef wdef;
    wdef.gravity = Vector2D(0.0f, -9.81f);
    wdef.fixedTimestep = 1.0f / 60.0f;
    wdef.maxSubSteps = 8;
    wdef.solverIterations = 10;
    wdef.rigidBodyWorld = nullptr;
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&floor);

    ClothDef cdef = MakeTestClothDef();
    cdef.origin = Vector2D(-1.0f, 0.0f);
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 120; ++i)
        world.Update(wdef.fixedTimestep);

    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            EXPECT_GE(cloth->GetParticle(x, y).position.y, -1.0f - cloth->GetParticle(x, y).radius - 0.5f);
        }
    }

    world.RemoveStaticShape(&floor);
}

TEST(SoftBody2D_GeometryCollision, ClothVsCircle_ParticlesOutside)
{
    Dia::Geometry2D::Circle obstacle(0.5f, Vector2D(1.0f, -1.0f));

    WorldDef wdef = MakeWorldDef();
    wdef.gravity = Vector2D(0.0f, -9.81f);
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&obstacle);

    ClothDef cdef = MakeTestClothDef();
    cdef.origin = Vector2D(0.0f, 0.0f);
    world.AddCloth(cdef);

    for (int i = 0; i < 60; ++i)
        world.Update(wdef.fixedTimestep);

    SUCCEED();

    world.RemoveStaticShape(&obstacle);
}

TEST(SoftBody2D_GeometryCollision, MultipleStaticShapes_AllActive)
{
    Dia::Geometry2D::AARect floor(Vector2D(-5.0f, -3.0f), Vector2D(5.0f, -2.0f));
    Dia::Geometry2D::Circle pillar(0.5f, Vector2D(0.0f, -1.0f));
    Dia::Geometry2D::Line wall(Vector2D(3.0f, -5.0f), Vector2D(3.0f, 5.0f));

    WorldDef wdef = MakeWorldDef();
    wdef.gravity = Vector2D(0.0f, -9.81f);
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&floor);
    world.AddStaticShape(&pillar);
    world.AddStaticShape(&wall);

    world.AddRope(MakeTestRopeDef());

    for (int i = 0; i < 60; ++i)
        world.Update(wdef.fixedTimestep);

    SUCCEED();

    world.RemoveStaticShape(&floor);
    world.RemoveStaticShape(&pillar);
    world.RemoveStaticShape(&wall);
}

TEST(SoftBody2D_GeometryCollision, ParticleFullyInsideAARect_DegenerateNormal)
{
    Dia::Geometry2D::AARect bigRect(Vector2D(-10.0f, -10.0f), Vector2D(10.0f, 10.0f));

    WorldDef wdef = MakeWorldDef();
    SoftBodyWorld world(wdef);
    world.AddStaticShape(&bigRect);

    RopeDef rdef = MakeTestRopeDef();
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.5f, 0.0f);
    world.AddRope(rdef);

    world.Update(wdef.fixedTimestep);
    SUCCEED();

    world.RemoveStaticShape(&bigRect);
}
