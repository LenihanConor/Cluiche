#include "CalloutRadiiDrawer.h"
#ifdef DIA_DEBUG

#include <DiaAICallout/CalloutRegistry.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Colour/RGBA.h>

const Dia::Core::StringCRC Dia::AICalloutVisualDebugger::CalloutRadiiDrawer::kLayerName("aicallout.radii");

namespace Dia::AICalloutVisualDebugger {

CalloutRadiiDrawer::CalloutRadiiDrawer(const Dia::AICallout::CalloutRegistry& registry)
    : mRegistry(registry)
{}

Dia::Core::StringCRC CalloutRadiiDrawer::GetLayerName() const
{
    return kLayerName;
}

void CalloutRadiiDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    for (uint32_t i = 0u; i < Dia::AICallout::CalloutRegistryData::kMaxCallouts; ++i)
    {
        const Dia::AICallout::CalloutSlot& slot = mRegistry.mSlots[i];
        if (!slot.live) continue;

        const Dia::Core::RGBA colour = slot.claimed
            ? Dia::Core::RGBA(0xef, 0x44, 0x44, 0xCC)
            : Dia::Core::RGBA(0x10, 0xb9, 0x81, 0xCC);

        draw.RequestDraw(slot.callout.position, slot.callout.radius, colour);
    }
}

} // namespace Dia::AICalloutVisualDebugger

#endif // DIA_DEBUG
