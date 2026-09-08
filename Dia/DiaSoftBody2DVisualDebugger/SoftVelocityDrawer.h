////////////////////////////////////////////////////////////////////////////////
// Filename: SoftVelocityDrawer.h
// Description: IVisualDebugger that draws the Verlet velocity (position delta)
//              of each dynamic particle as a line segment.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::SoftBody2D
{

class SoftVelocityDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftVelocityDrawer(const SoftBodyWorld&            world,
                       const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const SoftBodyWorld&                 mWorld;
    const Dia::Core::IDebugContext& mManager;
    float                                mVelocityScale = 1.0f;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
