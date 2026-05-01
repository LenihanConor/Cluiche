#pragma once

#include "DiaRigidBody2D/Bodies/BodyType.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Core/Assert.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::Geometry2D { class Transform; class AARect; class Circle; class ConvexPolygon; }

namespace Dia::RigidBody2D {

struct Body2DDef {
    Dia::Core::StringCRC              id;
    const Dia::Geometry2D::Transform*     transform   = nullptr;   // Initial position/rotation; body copies on construction
    const Dia::Geometry2D::Circle*        circleShape = nullptr;   // Origin-centered circle; world pos comes from transform
    const Dia::Geometry2D::ConvexPolygon* polyShape   = nullptr;   // Origin-centered convex polygon
    BodyType type            = BodyType::kDynamic;
    float    mass            = 1.0f;
    float    restitution     = 0.2f;   // Bounciness [0..1]
    float    friction        = 0.5f;   // Coulomb friction coefficient
    float    linearDamping   = 0.0f;   // Per-step linear velocity decay
    bool     allowSleeping   = true;
    uint32_t layer           = 0x1;          // Single-bit collision layer (e.g. Layers::kPlayer)
    uint32_t mask            = 0xFFFFFFFF;   // Bitmask of layers this body collides with
};

class Body2DBase {
public:
    virtual ~Body2DBase() = default;

    BodyType                    GetBodyType()    const { return mType; }
    ShapeKind                   GetShapeKind()   const { return mShapeKind; }
    float                       GetInverseMass() const { return mInvMass; }
    float                       GetRestitution() const { return mRestitution; }
    float                       GetFriction()    const { return mFriction; }
    const Dia::Core::StringCRC& GetId()          const { return mId; }

    uint32_t GetUniqueId()              const { return mUniqueId; }
    void     SetUniqueId(uint32_t uid)        { mUniqueId = uid; }

    virtual Dia::Geometry2D::Transform*       GetTransform()                  = 0;
    virtual const Dia::Geometry2D::Transform* GetTransform()            const = 0;
    virtual const Dia::Geometry2D::Circle*        GetCircleShape()      const = 0;
    virtual const Dia::Geometry2D::ConvexPolygon* GetPolyShape()        const = 0;

    virtual const Dia::Maths::Vector2D& GetVelocity()        const = 0;
    virtual float                       GetAngularVelocity() const { return 0.0f; }
    virtual float                       GetInverseInertia()  const { return 0.0f; }

    virtual void ApplyImpulse(const Dia::Maths::Vector2D& impulse)      = 0;
    virtual void ApplyAngularImpulse(float impulse)                     { (void)impulse; }
    virtual void ApplyForce(const Dia::Maths::Vector2D& force)          = 0;
    virtual void SetVelocity(const Dia::Maths::Vector2D& vel)           = 0;
    virtual void ClearForces()                                          = 0;

    SleepState GetSleepState() const { return mSleepState; }
    bool       IsAwake()       const { return mSleepState == SleepState::kAwake; }

    void Wake()
    {
        mSleepState = SleepState::kAwake;
        mSleepTimer = 0.0f;
    }

    void Sleep()
    {
        if (!mAllowSleeping) return;
        mSleepState = SleepState::kSleeping;
    }

    bool  AllowsSleeping() const { return mAllowSleeping; }
    float GetSleepTimer()  const { return mSleepTimer; }

    void SetSleepTimer(float t)       { mSleepTimer  = t; }
    void SetSleepState(SleepState ss) { mSleepState  = ss; }

    uint32_t GetLayer() const { return mLayer; }
    uint32_t GetMask()  const { return mMask; }

    void SetLayer(uint32_t layer)
    {
        DIA_ASSERT((layer & (layer - 1)) == 0, "Layer must have exactly one bit set");
        mLayer = layer;
    }

    void SetMask(uint32_t mask) { mMask = mask; }

protected:
    explicit Body2DBase(const Body2DDef& def);

    Dia::Core::StringCRC mId;
    uint32_t             mUniqueId     = 0;          // Stable identity assigned by PhysicsWorld; used in BodyPairKey hashing
    BodyType             mType         = BodyType::kDynamic;
    float                mMass         = 1.0f;
    float                mInvMass      = 1.0f;
    float                mRestitution  = 0.2f;       // Bounciness [0..1]; collision uses min(a,b)
    float                mFriction     = 0.5f;       // Coulomb friction coefficient; collision uses sqrt(a*b)
    SleepState           mSleepState   = SleepState::kAwake;
    float                mSleepTimer   = 0.0f;
    bool                 mAllowSleeping = true;
    ShapeKind            mShapeKind    = ShapeKind::kNone;
    uint32_t             mLayer        = 0x1;         // Single-bit flag identifying this body's collision layer
    uint32_t             mMask         = 0xFFFFFFFF;  // Bitmask of layers this body can collide with
};

} // namespace Dia::RigidBody2D
