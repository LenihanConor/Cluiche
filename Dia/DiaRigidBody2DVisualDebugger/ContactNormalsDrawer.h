////////////////////////////////////////////////////////////////////////////////
// Filename: ContactNormalsDrawer.h
// Description: IVisualDebugger that draws a ray at each contact point showing
//              the collision normal from the last simulation step.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::Debug       { class DebugLayerManager; }

namespace Dia::RigidBody2D
{

class ContactNormalsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    ContactNormalsDrawer(const PhysicsWorld&                world,
                         const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const PhysicsWorld&                  mWorld;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
