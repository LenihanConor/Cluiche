////////////////////////////////////////////////////////////////////////////////
// Filename: SoftParticlesDrawer.h
// Description: IVisualDebugger that draws a circle at each soft-body particle
//              position, coloured by pinning state.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Debug       { class DebugLayerManager; }

namespace Dia::SoftBody2D
{

class SoftParticlesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftParticlesDrawer(const SoftBodyWorld&                world,
                        const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const SoftBodyWorld&                 mWorld;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
