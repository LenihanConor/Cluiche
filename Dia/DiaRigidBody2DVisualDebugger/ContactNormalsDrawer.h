////////////////////////////////////////////////////////////////////////////////
// Filename: ContactNormalsDrawer.h
// Description: IVisualDebugger that draws a ray at each contact point showing
//              the collision normal from the last simulation step.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::Core        { class IDebugContext; }

namespace Dia::RigidBody2D
{

class ContactNormalsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    ContactNormalsDrawer(const PhysicsWorld&             world,
                         const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const PhysicsWorld&                  mWorld;
    const Dia::Core::IDebugContext& mManager;
    float                                mNormalLength = 0.3f;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
