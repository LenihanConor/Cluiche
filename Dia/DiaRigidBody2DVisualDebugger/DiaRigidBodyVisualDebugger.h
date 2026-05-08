#pragma once

namespace Dia::Graphics { class FrameData; }
namespace Dia::RigidBody2D { class PhysicsWorld; }

namespace Dia::RigidBody2D {

struct VisualDebuggerOptions {
    float velocityArrowScale  = 0.1f;   // world-units per unit of speed
    float velocityArrowMaxLen = 10.0f;  // cap in world-units
};

class DiaRigidBodyVisualDebugger {
public:
    DiaRigidBodyVisualDebugger();

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    void SetOptions(const VisualDebuggerOptions& options);
    const VisualDebuggerOptions& GetOptions() const;

    void Draw(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);

private:
    bool                   mEnabled = false;
    VisualDebuggerOptions  mOptions;

    void DrawBodies(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawVelocityArrows(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawContactNormals(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawConstraints(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
};

} // namespace Dia::RigidBody2D
