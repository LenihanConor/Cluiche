////////////////////////////////////////////////////////////////////////////////
// Filename: ContactNormalsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::RigidBody2D
{


ContactNormalsDrawer::ContactNormalsDrawer(const PhysicsWorld&                world,
                                           const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC ContactNormalsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kPhysicsContacts;
}

void ContactNormalsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    DIA_TRACE_ZONE("physics.contacts", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale    = mManager.GetDebugScale();
    const auto& contacts = mWorld.GetLastContacts();

    for (unsigned int i = 0; i < contacts.Size(); ++i)
    {
        const Contact& c = contacts[i];
        frameData.RequestDrawRay(
            c.point,
            c.normal,
            mNormalLength * scale,
            Dia::Debug::DebugColourPalette::kError);
    }
}

void ContactNormalsDrawer::DrawImGui()
{
    ImGui::SliderFloat("Normal length", &mNormalLength, 0.05f, 2.0f);
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
