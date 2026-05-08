////////////////////////////////////////////////////////////////////////////////
// Filename: PhysicsShapesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h"

#ifdef DIA_DEBUG

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaRigidBody2D/Bodies/BodyType.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

#include <cmath>

namespace Dia::RigidBody2D
{

static Dia::Maths::Vector2D RotateVec(const Dia::Maths::Vector2D& v, float rad)
{
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    return Dia::Maths::Vector2D(c * v.x - s * v.y, s * v.x + c * v.y);
}

static Dia::Graphics::RGBA BodyColour(const Body2DBase* body)
{
    if (!body->IsAwake())
        return Dia::Debug::DebugColourPalette::kDeepSleep;
    switch (body->GetBodyType())
    {
        case BodyType::kDynamic:   return Dia::Debug::DebugColourPalette::kActive;
        case BodyType::kStatic:    return Dia::Debug::DebugColourPalette::kInactive;
        case BodyType::kKinematic: return Dia::Debug::DebugColourPalette::kGoal;
    }
    return Dia::Debug::DebugColourPalette::kActive;
}

static void DrawBody(const Body2DBase* body, Dia::Graphics::FrameData& frameData)
{
    const Dia::Geometry2D::Transform* t = body->GetTransform();
    if (!t) return;

    const Dia::Maths::Vector2D pos = t->GetWorldPosition();
    const float                rot = t->GetLocalRotation().AsRadians();
    const Dia::Graphics::RGBA  col = BodyColour(body);

    switch (body->GetShapeKind())
    {
        case ShapeKind::kCircle:
        {
            const Dia::Geometry2D::Circle* c = body->GetCircleShape();
            if (c)
                frameData.RequestDraw(pos, c->GetRadius(), col);
            break;
        }
        case ShapeKind::kPoly:
        {
            const Dia::Geometry2D::ConvexPolygon* poly = body->GetPolyShape();
            if (poly && poly->GetVertexCount() >= 2)
            {
                const int n = poly->GetVertexCount();
                for (int i = 0; i < n; ++i)
                {
                    const Dia::Maths::Vector2D lv0 = poly->GetVertex(i);
                    const Dia::Maths::Vector2D lv1 = poly->GetVertex((i + 1) % n);
                    const Dia::Maths::Vector2D wv0 = pos + RotateVec(lv0, rot);
                    const Dia::Maths::Vector2D wv1 = pos + RotateVec(lv1, rot);
                    frameData.RequestDraw(wv0, wv1, col);
                }
            }
            break;
        }
        default:
            frameData.RequestDrawPoint(pos, col);
            break;
    }
}

// ---------------------------------------------------------------------------

PhysicsShapesDrawer::PhysicsShapesDrawer(const PhysicsWorld&                world,
                                         const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC PhysicsShapesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kPhysicsShapes;
}

void PhysicsShapesDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const auto& pointBodies = mWorld.GetPointBodies();
    const auto& rigidBodies = mWorld.GetRigidBodies();

    for (unsigned int i = 0; i < pointBodies.Size(); ++i)
        DrawBody(pointBodies[i], frameData);

    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
        DrawBody(rigidBodies[i], frameData);
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
