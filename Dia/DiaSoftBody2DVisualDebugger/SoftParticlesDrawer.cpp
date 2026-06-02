////////////////////////////////////////////////////////////////////////////////
// Filename: SoftParticlesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSoftBody2DVisualDebugger/SoftParticlesDrawer.h"

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
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

namespace Dia::SoftBody2D
{

SoftParticlesDrawer::SoftParticlesDrawer(const SoftBodyWorld&                world,
                                         const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC SoftParticlesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kSoftParticles;
}

static void DrawParticlesFromRope(const Rope* rope, float scale,
                                  Dia::Graphics::FrameData& frameData)
{
    const int count = rope->GetParticleCount();
    for (int i = 0; i < count; ++i)
    {
        const Particle& p = rope->GetParticle(i);
        const Dia::Graphics::RGBA colour = (p.invMass == 0.0f)
            ? Dia::Debug::DebugColourPalette::kPinned
            : Dia::Debug::DebugColourPalette::kActive;
        frameData.RequestDraw(p.position, p.radius * scale, colour);
    }
}

static void DrawParticlesFromCloth(const Cloth* cloth, float scale,
                                   Dia::Graphics::FrameData& frameData)
{
    const int resX = cloth->GetResX();
    const int resY = cloth->GetResY();
    for (int y = 0; y < resY; ++y)
    {
        for (int x = 0; x < resX; ++x)
        {
            const Particle& p = cloth->GetParticle(x, y);
            const Dia::Graphics::RGBA colour = (p.invMass == 0.0f)
                ? Dia::Debug::DebugColourPalette::kPinned
                : Dia::Debug::DebugColourPalette::kActive;
            frameData.RequestDraw(p.position, p.radius * scale, colour);
        }
    }
}

void SoftParticlesDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    DIA_TRACE_ZONE("soft.particles", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float debugScale = mManager.GetDebugScale() * mRadiusMultiplier;
    const auto& bodies = mWorld.GetBodies();

    for (unsigned int b = 0; b < bodies.Size(); ++b)
    {
        const SoftBody* body = bodies[b];
        DIA_ASSERT(body != nullptr, "SoftBodyWorld contains null body pointer");

        switch (body->GetBodyType())
        {
            case BodyType::kRope:
                DrawParticlesFromRope(static_cast<const Rope*>(body), debugScale, frameData);
                break;
            case BodyType::kCloth:
                DrawParticlesFromCloth(static_cast<const Cloth*>(body), debugScale, frameData);
                break;
        }
    }
}

void SoftParticlesDrawer::DrawImGui()
{
    ImGui::SliderFloat("Radius multiplier", &mRadiusMultiplier, 0.1f, 5.0f);
    ImGui::TextDisabled("Bodies: %u", mWorld.GetBodies().Size());
}

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
