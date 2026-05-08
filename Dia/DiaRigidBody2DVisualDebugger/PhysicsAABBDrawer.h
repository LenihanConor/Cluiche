////////////////////////////////////////////////////////////////////////////////
// Filename: PhysicsAABBDrawer.h
// Description: IVisualDebugger that draws the world-space AABB for each rigid body.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::Debug       { class DebugLayerManager; }

namespace Dia::RigidBody2D
{

class PhysicsAABBDrawer : public Dia::Debug::IVisualDebugger
{
public:
    PhysicsAABBDrawer(const PhysicsWorld&                world,
                      const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const PhysicsWorld&                  mWorld;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
