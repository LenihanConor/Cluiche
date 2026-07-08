////////////////////////////////////////////////////////////////////////////////
// Filename: SoftParticlesDrawer.h
// Description: IVisualDebugger that draws a circle at each soft-body particle
//              position, coloured by pinning state.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::SoftBody2D
{

class SoftParticlesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftParticlesDrawer(const SoftBodyWorld&            world,
                        const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

    float mRadiusMultiplier = 1.0f;

private:
    const SoftBodyWorld&                 mWorld;
    const Dia::Core::IDebugContext& mManager;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
