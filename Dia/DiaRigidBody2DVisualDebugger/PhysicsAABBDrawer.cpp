////////////////////////////////////////////////////////////////////////////////
// Filename: PhysicsAABBDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h"

#ifdef DIA_DEBUG

#include <imgui.h>

#include <DiaObservation/Trace/DiaTrace.h>
#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/WorldShapeUtil.h"
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Colour/RGBA.h>
#include "DiaGeometry2D/Shapes/AARect.h"
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>

namespace Dia::RigidBody2D
{

PhysicsAABBDrawer::PhysicsAABBDrawer(const PhysicsWorld&             world,
                                     const Dia::Core::IDebugContext& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC PhysicsAABBDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kPhysicsAABB;
}

void PhysicsAABBDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("physics.aabb", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const auto& rigidBodies = mWorld.GetRigidBodies();

    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
    {
        const Body2DBase* body = rigidBodies[i];
        if (!body->GetTransform()) continue;

        const Dia::Geometry2D::AARect aabb = ComputeWorldAABB(body);
        if (mFilled)
        {
            draw.RequestDrawRect(
                aabb.GetBottomLeft(),
                aabb.GetTopRight(),
                Dia::Debug::DebugColourPalette::kWarning,
                Dia::Core::RGBA(255, 220, 0, 40));
        }
        else
        {
            draw.RequestDrawRect(
                aabb.GetBottomLeft(),
                aabb.GetTopRight(),
                Dia::Debug::DebugColourPalette::kWarning);
        }
    }
}

void PhysicsAABBDrawer::DrawImGui()
{
    ImGui::Checkbox("Filled", &mFilled);
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
