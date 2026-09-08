////////////////////////////////////////////////////////////////////////////////
// Filename: PhysicsAABBDrawer.h
// Description: IVisualDebugger that draws the world-space AABB for each rigid body.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::RigidBody2D
{

class PhysicsAABBDrawer : public Dia::Debug::IVisualDebugger
{
public:
    PhysicsAABBDrawer(const PhysicsWorld&             world,
                      const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

    bool GetFilled() const { return mFilled; }

private:
    const PhysicsWorld&                  mWorld;
    [[maybe_unused]] const Dia::Core::IDebugContext& mManager;
    bool                                 mFilled = false;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
