////////////////////////////////////////////////////////////////////////////////
// Filename: SoftAnchorLinksDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSoftBody2DVisualDebugger/SoftAnchorLinksDrawer.h"

#ifdef DIA_DEBUG

#include "DiaSoftBody2D/SoftBodyWorld.h"
#include "DiaSoftBody2D/SoftBody.h"
#include "DiaSoftBody2D/Rope.h"
#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"
#include "DiaCore/Core/Assert.h"

namespace Dia::SoftBody2D
{

SoftAnchorLinksDrawer::SoftAnchorLinksDrawer(const SoftBodyWorld&                world,
                                             const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC SoftAnchorLinksDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kSoftAnchors;
}

void SoftAnchorLinksDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const auto& bodies = mWorld.GetBodies();

    for (unsigned int b = 0; b < bodies.Size(); ++b)
    {
        const SoftBody* body = bodies[b];
        DIA_ASSERT(body != nullptr, "SoftBodyWorld contains null body pointer");

        if (body->GetBodyType() != BodyType::kRope) continue;

        const Rope* rope = static_cast<const Rope*>(body);
        const int count  = rope->GetParticleCount();
        if (count == 0) continue;

        const Dia::RigidBody2D::Body2DBase* startAnchor = rope->GetStartAnchor();
        if (startAnchor != nullptr)
        {
            const Dia::Geometry2D::Transform* t = startAnchor->GetTransform();
            if (t != nullptr)
            {
                const Dia::Maths::Vector2D& particlePos  = rope->GetParticle(0).position;
                const Dia::Maths::Vector2D  anchorWorldPos = t->GetWorldPosition();
                frameData.RequestDraw(particlePos, anchorWorldPos,
                                      Dia::Debug::DebugColourPalette::kWarning);
            }
        }

        const Dia::RigidBody2D::Body2DBase* endAnchor = rope->GetEndAnchor();
        if (endAnchor != nullptr)
        {
            const Dia::Geometry2D::Transform* t = endAnchor->GetTransform();
            if (t != nullptr)
            {
                const Dia::Maths::Vector2D& particlePos  = rope->GetParticle(count - 1).position;
                const Dia::Maths::Vector2D  anchorWorldPos = t->GetWorldPosition();
                frameData.RequestDraw(particlePos, anchorWorldPos,
                                      Dia::Debug::DebugColourPalette::kWarning);
            }
        }
    }
}

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
