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
#include "DiaCore/DebugDraw/IDebugDraw.h"
#include "DiaCore/DebugDraw/IDebugContext.h"
#include "DiaCore/DebugDraw/DebugColourPalette.h"
#include "DiaCore/DebugDraw/DebugLayerNames.h"
#include "DiaCore/Core/Assert.h"

#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>
#include <cmath>

namespace Dia::SoftBody2D
{

SoftVelocityDrawer::SoftVelocityDrawer(const SoftBodyWorld&            world,
                                       const Dia::Core::IDebugContext& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC SoftVelocityDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kSoftVelocity;
}

static void DrawParticleVelocity(const Particle& p, float scale,
                                  Dia::Core::IDebugDraw& draw)
{
    if (p.invMass == 0.0f) return;

    // Verlet velocity = position - prevPosition
    const Dia::Maths::Vector2D delta = p.position - p.prevPosition;
    const float magnitude = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (magnitude < 1e-4f) return;

    draw.RequestDraw(p.position, p.position + delta * scale,
                          Dia::Debug::DebugColourPalette::kHealthy);
}

static void DrawVelocityFromRope(const Rope* rope, float scale,
                                  Dia::Core::IDebugDraw& draw)
{
    const int count = rope->GetParticleCount();
    for (int i = 0; i < count; ++i)
        DrawParticleVelocity(rope->GetParticle(i), scale, draw);
}

static void DrawVelocityFromCloth(const Cloth* cloth, float scale,
                                   Dia::Core::IDebugDraw& draw)
{
    const int resX = cloth->GetResX();
    const int resY = cloth->GetResY();
    for (int y = 0; y < resY; ++y)
        for (int x = 0; x < resX; ++x)
            DrawParticleVelocity(cloth->GetParticle(x, y), scale, draw);
}

void SoftVelocityDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("soft.velocity", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale = mManager.GetDebugScale() * mVelocityScale;
    const auto& bodies = mWorld.GetBodies();

    for (unsigned int b = 0; b < bodies.Size(); ++b)
    {
        const SoftBody* body = bodies[b];
        DIA_ASSERT(body != nullptr, "SoftBodyWorld contains null body pointer");

        switch (body->GetBodyType())
        {
            case BodyType::kRope:
                DrawVelocityFromRope(static_cast<const Rope*>(body), scale, draw);
                break;
            case BodyType::kCloth:
                DrawVelocityFromCloth(static_cast<const Cloth*>(body), scale, draw);
                break;
        }
    }
}

void SoftVelocityDrawer::DrawImGui()
{
    ImGui::SliderFloat("Velocity scale", &mVelocityScale, 0.1f, 5.0f);
}

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
