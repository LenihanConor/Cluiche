////////////////////////////////////////////////////////////////////////////////
// Filename: PhysicsAABBDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h"

#ifdef DIA_DEBUG

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/WorldShapeUtil.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::RigidBody2D
{

PhysicsAABBDrawer::PhysicsAABBDrawer(const PhysicsWorld&                world,
                                     const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC PhysicsAABBDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kPhysicsAABB;
}

void PhysicsAABBDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const auto& rigidBodies = mWorld.GetRigidBodies();

    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
    {
        const Body2DBase* body = rigidBodies[i];
        if (!body->GetTransform()) continue;

        const Dia::Geometry2D::AARect aabb = ComputeWorldAABB(body);
        frameData.RequestDrawRect(
            aabb.GetBottomLeft(),
            aabb.GetTopRight(),
            Dia::Debug::DebugColourPalette::kWarning);
    }
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
