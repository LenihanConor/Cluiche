////////////////////////////////////////////////////////////////////////////////
// Filename: SoftConstraintsDrawer.h
// Description: IVisualDebugger that draws a line between particle pairs for
//              each active constraint, colour-coded by DistanceConstraint::type.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Debug       { class DebugLayerManager; }

namespace Dia::SoftBody2D
{

class SoftConstraintsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftConstraintsDrawer(const SoftBodyWorld&                world,
                          const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const SoftBodyWorld&                 mWorld;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
