////////////////////////////////////////////////////////////////////////////////
// Filename: ContactNormalsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h"

#ifdef DIA_DEBUG

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"

namespace Dia::RigidBody2D
{

static constexpr float kContactNormalLength = 0.3f;

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
    const float scale    = mManager.GetDebugScale();
    const auto& contacts = mWorld.GetLastContacts();

    for (unsigned int i = 0; i < contacts.Size(); ++i)
    {
        const Contact& c = contacts[i];
        frameData.RequestDrawRay(
            c.point,
            c.normal,
            kContactNormalLength * scale,
            Dia::Debug::DebugColourPalette::kError);
    }
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
