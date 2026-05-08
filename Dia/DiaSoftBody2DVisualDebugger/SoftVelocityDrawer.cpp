////////////////////////////////////////////////////////////////////////////////
// Filename: SoftVelocityDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSoftBody2DVisualDebugger/SoftVelocityDrawer.h"

#ifdef DIA_DEBUG

#include "DiaSoftBody2D/SoftBodyWorld.h"
#include "DiaSoftBody2D/SoftBody.h"
#include "DiaSoftBody2D/Rope.h"
#include "DiaSoftBody2D/Cloth.h"
#include "DiaSoftBody2D/Particle.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"
#include "DiaCore/Core/Assert.h"

#include <cmath>

namespace Dia::SoftBody2D
{

SoftVelocityDrawer::SoftVelocityDrawer(const SoftBodyWorld&                world,
                                       const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC SoftVelocityDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kSoftVelocity;
}

static void DrawParticleVelocity(const Particle& p, Dia::Graphics::FrameData& frameData)
{
    // Skip pinned particles
    if (p.invMass == 0.0f) return;

    // Verlet velocity = position - prevPosition
    const Dia::Maths::Vector2D delta = p.position - p.prevPosition;
    const float magnitude = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (magnitude < 1e-4f) return;

    frameData.RequestDraw(p.position, p.position + delta,
                          Dia::Debug::DebugColourPalette::kHealthy);
}

static void DrawVelocityFromRope(const Rope* rope, Dia::Graphics::FrameData& frameData)
{
    const int count = rope->GetParticleCount();
    for (int i = 0; i < count; ++i)
        DrawParticleVelocity(rope->GetParticle(i), frameData);
}

static void DrawVelocityFromCloth(const Cloth* cloth, Dia::Graphics::FrameData& frameData)
{
    const int resX = cloth->GetResX();
    const int resY = cloth->GetResY();
    for (int y = 0; y < resY; ++y)
        for (int x = 0; x < resX; ++x)
            DrawParticleVelocity(cloth->GetParticle(x, y), frameData);
}

void SoftVelocityDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const auto& bodies = mWorld.GetBodies();

    for (unsigned int b = 0; b < bodies.Size(); ++b)
    {
        const SoftBody* body = bodies[b];
        DIA_ASSERT(body != nullptr, "SoftBodyWorld contains null body pointer");

        switch (body->GetBodyType())
        {
            case BodyType::kRope:
                DrawVelocityFromRope(static_cast<const Rope*>(body), frameData);
                break;
            case BodyType::kCloth:
                DrawVelocityFromCloth(static_cast<const Cloth*>(body), frameData);
                break;
        }
    }
}

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
