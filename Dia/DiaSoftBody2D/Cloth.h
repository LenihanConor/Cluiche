#pragma once

#include "DiaSoftBody2D/SoftBody.h"
#include "DiaSoftBody2D/Particle.h"
#include "DiaSoftBody2D/Constraints/DistanceConstraint.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArray.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::SoftBody2D {

static constexpr int kMaxClothParticles = 4096;

struct ClothDef {
    Dia::Core::StringCRC  id;
    Dia::Maths::Vector2D  origin;
    float                 width;
    float                 height;
    int                   resX;
    int                   resY;
    float                 mass                  = 1.0f;
    float                 structuralStiffness   = 1.0f;
    float                 shearStiffness        = 0.8f;
    float                 bendStiffness         = 0.3f;
    float                 particleRadius        = 0.05f;
    float                 maxStretch            = 0.0f;
    bool                  pinTopRow             = false;
};

class Cloth : public SoftBody {
public:
    static const Dia::Core::StringCRC kUniqueId;

    explicit Cloth(const ClothDef& def);

    int              GetParticleCount() const;
    Particle&        GetParticle(int x, int y);
    const Particle&  GetParticle(int x, int y) const;

    int                        GetConstraintCount() const;
    DistanceConstraint&        GetConstraint(int index);
    const DistanceConstraint&  GetConstraint(int index) const;

    void PinParticle(int x, int y);
    void UnpinParticle(int x, int y);

    bool                        IsTorn() const;
    const Dia::Core::StringCRC& GetId() const override;
    BodyType                    GetBodyType() const override;

    int   GetResX() const;
    int   GetResY() const;
    float GetMaxStretch() const;

    void  CheckTearing();

private:
    int ParticleIndex(int x, int y) const;

    void AddConstraint(int idxA, int idxB, float restLen, float stiffness, ConstraintType type);

    Dia::Core::StringCRC                                    mId;
    int                                                     mResX;
    int                                                     mResY;
    Dia::Core::Containers::DynamicArray<Particle>           mParticles;
    Dia::Core::Containers::DynamicArray<float>              mOriginalInvMass;
    Dia::Core::Containers::DynamicArray<DistanceConstraint> mConstraints;
    bool                                                    mIsTorn;
    float                                                   mMaxStretch;
};

} // namespace Dia::SoftBody2D
