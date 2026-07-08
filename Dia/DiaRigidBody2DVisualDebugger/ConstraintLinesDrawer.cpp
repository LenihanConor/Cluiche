////////////////////////////////////////////////////////////////////////////////
// Filename: ConstraintLinesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::RigidBody2D
{

ConstraintLinesDrawer::ConstraintLinesDrawer(const PhysicsWorld&                world,
                                             const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC ConstraintLinesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kPhysicsConstraints;
}

void ConstraintLinesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("physics.constraints", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const auto& constraints = mWorld.GetConstraints();

    for (unsigned int i = 0; i < constraints.Size(); ++i)
    {
        const IConstraint* c       = constraints[i];
        const Dia::Maths::Vector2D anchorA = c->GetWorldAnchorA();
        const Dia::Maths::Vector2D anchorB = c->GetWorldAnchorB();
        draw.RequestDraw(anchorA, anchorB, Dia::Debug::DebugColourPalette::kGoal);
    }
}

void ConstraintLinesDrawer::DrawImGui()
{
    ImGui::TextDisabled("Constraints: %u", mWorld.GetConstraints().Size());
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
