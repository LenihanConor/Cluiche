#pragma once

#include "DiaSoftBody2D/SoftBody.h"
#include "DiaSoftBody2D/Particle.h"
#include "DiaSoftBody2D/Constraints/DistanceConstraint.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D { class Body2DBase; }

namespace Dia::SoftBody2D {

static constexpr int kMaxRopeParticles    = 200;
static constexpr int kMaxRopeConstraints  = 199;

struct RopeDef {
    Dia::Core::StringCRC             id;
    Dia::Maths::Vector2D             startPoint;
    Dia::Maths::Vector2D             endPoint;
    int                              particleCount  = 10;
    float                            mass           = 1.0f;
    float                            stiffness      = 1.0f;
    float                            particleRadius = 0.1f;
    float                            maxStretch     = 0.0f;
    Dia::RigidBody2D::Body2DBase*    startAnchor    = nullptr;
    Dia::RigidBody2D::Body2DBase*    endAnchor      = nullptr;
};

class Rope : public SoftBody {
public:
    static const Dia::Core::StringCRC kUniqueId;

    explicit Rope(const RopeDef& def);

    int                                  GetParticleCount() const;
    Particle&                            GetParticle(int index);
    const Particle&                      GetParticle(int index) const;

    int                                  GetConstraintCount() const;
    DistanceConstraint&                  GetConstraint(int index);
    const DistanceConstraint&            GetConstraint(int index) const;

    bool                                 IsTorn() const;
    const Dia::Core::StringCRC&          GetId() const override;
    BodyType                             GetBodyType() const override;

    Dia::RigidBody2D::Body2DBase*        GetStartAnchor() const;
    Dia::RigidBody2D::Body2DBase*        GetEndAnchor() const;

    float                                GetStiffness() const;
    float                                GetMaxStretch() const;

    void                                 CheckTearing();

private:
    Dia::Core::StringCRC                                                    mId;
    Dia::Core::Containers::DynamicArrayC<Particle, kMaxRopeParticles>       mParticles;
    Dia::Core::Containers::DynamicArrayC<DistanceConstraint, kMaxRopeConstraints> mConstraints;
    bool                                                                    mIsTorn;
    float                                                                   mStiffness;
    float                                                                   mMaxStretch;
    Dia::RigidBody2D::Body2DBase*                                           mStartAnchor;
    Dia::RigidBody2D::Body2DBase*                                           mEndAnchor;
};

} // namespace Dia::SoftBody2D
