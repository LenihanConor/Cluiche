#include "SeparationRadiusDrawer.h"
#ifdef DIA_DEBUG

#include <DiaSteering/SteeringSystem.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>

namespace Dia { namespace Steering {

SeparationRadiusDrawer::SeparationRadiusDrawer(const SteeringSystem& system,
                                                const Dia::Core::IDebugContext& ctx)
    : mSystem(system), mCtx(ctx) {}

Dia::Core::StringCRC SeparationRadiusDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("Steering.SeparationRadius");
}

void SeparationRadiusDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    const float scale = mCtx.GetDebugScale();

    mSystem.VisitAgents([&](Dia::Core::StringCRC /*id*/,
                            const Dia::Steering::SteeringAgent& agent,
                            Dia::Maths::Vector2D /*desired*/)
    {
        if (agent.separationRadius <= 0.0f) return;
        draw.RequestDraw(agent.position,
                         agent.separationRadius * scale,
                         Dia::Debug::DebugColourPalette::kHealthy);
    });
}

} }
#endif // DIA_DEBUG
