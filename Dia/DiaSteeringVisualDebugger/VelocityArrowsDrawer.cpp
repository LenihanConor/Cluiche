#include "VelocityArrowsDrawer.h"
#ifdef DIA_DEBUG

#include <DiaSteering/SteeringSystem.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <cmath>

namespace Dia { namespace Steering {

VelocityArrowsDrawer::VelocityArrowsDrawer(const SteeringSystem& system,
                                            const Dia::Core::IDebugContext& ctx,
                                            float arrowScale)
    : mSystem(system), mCtx(ctx), mArrowScale(arrowScale) {}

Dia::Core::StringCRC VelocityArrowsDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("Steering.VelocityArrows");
}

void VelocityArrowsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;

    const float scale = mCtx.GetDebugScale() * mArrowScale;

    mSystem.VisitAgents([&](Dia::Core::StringCRC /*id*/,
                            const Dia::Steering::SteeringAgent& agent,
                            Dia::Maths::Vector2D desired)
    {
        // Current velocity arrow — yellow (kWarning)
        const float currentSpeedSq = agent.velocity.x * agent.velocity.x
                                   + agent.velocity.y * agent.velocity.y;
        if (currentSpeedSq > 1e-8f)
        {
            const float currentSpeed = std::sqrt(currentSpeedSq);
            const Dia::Maths::Vector2D currentDir(agent.velocity.x / currentSpeed,
                                                   agent.velocity.y / currentSpeed);
            draw.RequestDrawRay(agent.position, currentDir,
                                currentSpeed * scale,
                                Dia::Debug::DebugColourPalette::kWarning);
        }

        // Desired velocity arrow — cyan (kGoal)
        const float desiredSpeedSq = desired.x * desired.x + desired.y * desired.y;
        if (desiredSpeedSq > 1e-8f)
        {
            const float desiredSpeed = std::sqrt(desiredSpeedSq);
            const Dia::Maths::Vector2D desiredDir(desired.x / desiredSpeed,
                                                   desired.y / desiredSpeed);
            draw.RequestDrawRay(agent.position, desiredDir,
                                desiredSpeed * scale,
                                Dia::Debug::DebugColourPalette::kGoal);
        }
    });
}

} }
#endif // DIA_DEBUG
