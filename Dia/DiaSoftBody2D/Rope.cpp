#include "DiaSoftBody2D/Rope.h"

#include "DiaCore/Core/Assert.h"

namespace Dia::SoftBody2D {

const Dia::Core::StringCRC Rope::kUniqueId("Rope");

Rope::Rope(const RopeDef& def)
    : mId(def.id)
    , mIsTorn(false)
    , mStiffness(def.stiffness)
    , mMaxStretch(def.maxStretch)
    , mStartAnchor(def.startAnchor)
    , mEndAnchor(def.endAnchor)
{
    DIA_ASSERT(def.particleCount >= 2 && def.particleCount <= kMaxRopeParticles,
        "Rope particleCount must be in [2, %d]", kMaxRopeParticles);

    Dia::Maths::Vector2D delta = def.endPoint - def.startPoint;
    float totalLength = delta.Magnitude();
    float segmentLength = totalLength / static_cast<float>(def.particleCount - 1);
    float perParticleInvMass = (def.mass > 0.0f) ? static_cast<float>(def.particleCount) / def.mass : 0.0f;

    for (int i = 0; i < def.particleCount; ++i)
    {
        float t = (def.particleCount > 1) ? static_cast<float>(i) / static_cast<float>(def.particleCount - 1) : 0.0f;
        Particle p;
        p.position = Dia::Maths::Vector2D::Lerp(def.startPoint, def.endPoint, t);
        p.prevPosition = p.position;
        p.invMass = perParticleInvMass;
        p.radius = def.particleRadius;
        mParticles.Add(p);
    }

    if (mStartAnchor != nullptr)
    {
        mParticles[0].invMass = 0.0f;
    }

    if (mEndAnchor != nullptr)
    {
        mParticles[static_cast<int>(mParticles.Size()) - 1].invMass = 0.0f;
    }

    for (int i = 0; i < def.particleCount - 1; ++i)
    {
        DistanceConstraint c;
        c.indexA = i;
        c.indexB = i + 1;
        c.restLength = segmentLength;
        c.stiffness = def.stiffness;
        c.active = true;
        c.type = ConstraintType::kRope;
        mConstraints.Add(c);
    }
}

int Rope::GetParticleCount() const
{
    return static_cast<int>(mParticles.Size());
}

Particle& Rope::GetParticle(int index)
{
    DIA_ASSERT(index >= 0 && index < static_cast<int>(mParticles.Size()), "Particle index out of range");
    return mParticles[index];
}

const Particle& Rope::GetParticle(int index) const
{
    DIA_ASSERT(index >= 0 && index < static_cast<int>(mParticles.Size()), "Particle index out of range");
    return mParticles[index];
}

int Rope::GetConstraintCount() const
{
    return static_cast<int>(mConstraints.Size());
}

DistanceConstraint& Rope::GetConstraint(int index)
{
    DIA_ASSERT(index >= 0 && index < static_cast<int>(mConstraints.Size()), "Constraint index out of range");
    return mConstraints[index];
}

const DistanceConstraint& Rope::GetConstraint(int index) const
{
    DIA_ASSERT(index >= 0 && index < static_cast<int>(mConstraints.Size()), "Constraint index out of range");
    return mConstraints[index];
}

bool Rope::IsTorn() const
{
    return mIsTorn;
}

const Dia::Core::StringCRC& Rope::GetId() const
{
    return mId;
}

BodyType Rope::GetBodyType() const
{
    return BodyType::kRope;
}

Dia::RigidBody2D::Body2DBase* Rope::GetStartAnchor() const
{
    return mStartAnchor;
}

Dia::RigidBody2D::Body2DBase* Rope::GetEndAnchor() const
{
    return mEndAnchor;
}

float Rope::GetStiffness() const
{
    return mStiffness;
}

float Rope::GetMaxStretch() const
{
    return mMaxStretch;
}

void Rope::CheckTearing()
{
    if (mMaxStretch <= 0.0f)
    {
        return;
    }

    for (unsigned int i = 0; i < mConstraints.Size(); ++i)
    {
        DistanceConstraint& c = mConstraints[i];
        if (!c.active)
        {
            continue;
        }

        Dia::Maths::Vector2D delta = mParticles[c.indexB].position - mParticles[c.indexA].position;
        float dist = delta.Magnitude();

        if (c.restLength > 0.0f && dist > c.restLength * (1.0f + mMaxStretch))
        {
            c.active = false;
            mIsTorn = true;
        }
    }
}

} // namespace Dia::SoftBody2D
