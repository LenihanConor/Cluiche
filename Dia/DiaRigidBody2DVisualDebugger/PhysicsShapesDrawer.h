////////////////////////////////////////////////////////////////////////////////
// Filename: PhysicsShapesDrawer.h
// Description: IVisualDebugger that draws collision shapes for all bodies,
//              coloured by body state.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::RigidBody2D
{

class PhysicsShapesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    PhysicsShapesDrawer(const PhysicsWorld&             world,
                        const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

    bool GetShowSleeping() const { return mShowSleeping; }

private:
    const PhysicsWorld&                  mWorld;
    [[maybe_unused]] const Dia::Core::IDebugContext& mManager;
    bool                                 mShowSleeping = true;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
