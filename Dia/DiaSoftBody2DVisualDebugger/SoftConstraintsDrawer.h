////////////////////////////////////////////////////////////////////////////////
// Filename: SoftConstraintsDrawer.h
// Description: IVisualDebugger that draws a line between particle pairs for
//              each active constraint, colour-coded by DistanceConstraint::type.
// Feature spec: docs/specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::SoftBody2D { class SoftBodyWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::SoftBody2D
{

class SoftConstraintsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SoftConstraintsDrawer(const SoftBodyWorld&             world,
                          const Dia::Core::IDebugContext&  manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const SoftBodyWorld&                 mWorld;
    [[maybe_unused]] const Dia::Core::IDebugContext& mManager;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
