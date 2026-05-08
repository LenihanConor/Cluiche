////////////////////////////////////////////////////////////////////////////////
// Filename: VelocityArrowsDrawer.h
// Description: IVisualDebugger that draws a velocity arrow on each awake dynamic body.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }
namespace Dia::Debug       { class DebugLayerManager; }

namespace Dia::RigidBody2D
{

class VelocityArrowsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    VelocityArrowsDrawer(const PhysicsWorld&                world,
                         const Dia::Debug::DebugLayerManager& manager,
                         float arrowScale  = 0.1f,
                         float arrowMaxLen = 10.0f);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const PhysicsWorld&                  mWorld;
    const Dia::Debug::DebugLayerManager& mManager;
    float                                mArrowScale;
    float                                mArrowMaxLen;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
