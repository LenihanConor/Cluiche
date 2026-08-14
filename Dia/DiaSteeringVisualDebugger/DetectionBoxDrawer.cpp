#include "DetectionBoxDrawer.h"
#ifdef DIA_DEBUG

#include <DiaSteering/SteeringSystem.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>

namespace Dia { namespace Steering {

DetectionBoxDrawer::DetectionBoxDrawer(const SteeringSystem& system,
                                        const Dia::Core::IDebugContext& ctx)
    : mSystem(system), mCtx(ctx) {}

Dia::Core::StringCRC DetectionBoxDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("Steering.DetectionBoxes");
}

void DetectionBoxDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    const float scale = mCtx.GetDebugScale();

    mSystem.VisitAgents([&](Dia::Core::StringCRC /*id*/,
                            const Dia::Steering::SteeringAgent& agent,
                            Dia::Maths::Vector2D /*desired*/)
    {
        if (agent.detectionBoxLength <= 0.0f) return;

        const float half = agent.detectionBoxLength * 0.5f * scale;
        const Dia::Maths::Vector2D minPt(agent.position.x - half,
                                          agent.position.y - half);
        const Dia::Maths::Vector2D maxPt(agent.position.x + half,
                                          agent.position.y + half);
        draw.RequestDrawRect(minPt, maxPt,
                             Dia::Debug::DebugColourPalette::kCapped);
    });
}

} }
#endif // DIA_DEBUG
