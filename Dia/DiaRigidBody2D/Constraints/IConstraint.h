#pragma once

namespace Dia::RigidBody2D {

class RigidBody2D;

class IConstraint {
public:
    virtual ~IConstraint() = default;

    virtual void PreStep(float dt) = 0;
    virtual void ApplyImpulse() = 0;
    virtual bool InvolvesBody(const RigidBody2D* body) const = 0;
    virtual RigidBody2D* GetBodyA() const = 0;
    virtual RigidBody2D* GetBodyB() const = 0;
};

} // namespace Dia::RigidBody2D
