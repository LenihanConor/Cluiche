////////////////////////////////////////////////////////////////////////////////
// Filename: ConstraintLinesDrawer.h
// Description: IVisualDebugger that draws a line between the two world-space
//              anchor points of each constraint.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::RigidBody2D
{

class ConstraintLinesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    ConstraintLinesDrawer(const PhysicsWorld&             world,
                          const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const PhysicsWorld&                  mWorld;
    [[maybe_unused]] const Dia::Core::IDebugContext& mManager;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
