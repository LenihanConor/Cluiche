////////////////////////////////////////////////////////////////////////////////
// Filename: SoftVelocityDrawer.h
// Description: IVisualDebugger that draws the Verlet velocity (position delta)
//              of each dynamic particle as a line segment.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Debug       { class DebugLayerManager; }

namespace Dia::SoftBody2D
{

class SoftVelocityDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftVelocityDrawer(const SoftBodyWorld&                world,
                       const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const SoftBodyWorld&                 mWorld;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
