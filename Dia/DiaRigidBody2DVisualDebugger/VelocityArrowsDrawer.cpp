////////////////////////////////////////////////////////////////////////////////
// Filename: VelocityArrowsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.h"

#ifdef DIA_DEBUG

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaRigidBody2D/Bodies/BodyType.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

#include <cmath>

namespace Dia::RigidBody2D
{

VelocityArrowsDrawer::VelocityArrowsDrawer(const PhysicsWorld&                world,
                                           const Dia::Debug::DebugLayerManager& manager,
                                           float arrowScale,
                                           float arrowMaxLen)
    : mWorld(world)
    , mManager(manager)
    , mArrowScale(arrowScale)
    , mArrowMaxLen(arrowMaxLen)
{}

Dia::Core::StringCRC VelocityArrowsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kPhysicsVelocity;
}

void VelocityArrowsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const float debugScale = mManager.GetDebugScale();

    auto drawArrow = [&](const Body2DBase* body)
    {
        if (!body->IsAwake()) return;
        if (body->GetBodyType() == BodyType::kStatic) return;

        const Dia::Geometry2D::Transform* t = body->GetTransform();
        if (!t) return;

        const Dia::Maths::Vector2D pos   = t->GetWorldPosition();
        const Dia::Maths::Vector2D vel   = body->GetVelocity();
        const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (speed < 1e-4f) return;

        float len = speed * mArrowScale;
        if (len > mArrowMaxLen) len = mArrowMaxLen;
        len *= debugScale;

        const Dia::Maths::Vector2D dir{ vel.x / speed, vel.y / speed };
        frameData.RequestDrawRay(pos, dir, len, Dia::Debug::DebugColourPalette::kWarning);
    };

    const auto& pointBodies = mWorld.GetPointBodies();
    const auto& rigidBodies = mWorld.GetRigidBodies();

    for (unsigned int i = 0; i < pointBodies.Size(); ++i)
        drawArrow(pointBodies[i]);
    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
        drawArrow(rigidBodies[i]);
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
