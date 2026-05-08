#include "DiaSoftBody2D/Cloth.h"

#include "DiaCore/Core/Assert.h"
#include <DiaLogger/DiaLog.h>

#include <cmath>

namespace Dia::SoftBody2D {

const Dia::Core::StringCRC Cloth::kUniqueId("Cloth");

Cloth::Cloth(const ClothDef& def)
    : mId(def.id)
    , mResX(def.resX)
    , mResY(def.resY)
    , mIsTorn(false)
    , mMaxStretch(def.maxStretch)
{
    DIA_ASSERT(def.resX >= 2 && def.resY >= 2, "Cloth resolution must be >= 2 in each axis");
    DIA_ASSERT(def.resX * def.resY <= kMaxClothParticles,
        "Cloth particle count %d exceeds max %d", def.resX * def.resY, kMaxClothParticles);

    int totalParticles = def.resX * def.resY;
    float dx = (def.resX > 1) ? def.width  / static_cast<float>(def.resX - 1) : 0.0f;
    float dy = (def.resY > 1) ? def.height / static_cast<float>(def.resY - 1) : 0.0f;
    float perParticleInvMass = (def.mass > 0.0f) ? static_cast<float>(totalParticles) / def.mass : 0.0f;

    mParticles.Reserve(static_cast<unsigned int>(totalParticles));
    mOriginalInvMass.Reserve(static_cast<unsigned int>(totalParticles));

    int structuralH = def.resY * (def.resX - 1);
    int structuralV = (def.resY - 1) * def.resX;
    int shear = 2 * (def.resY - 1) * (def.resX - 1);
    int bendH = (def.resX >= 3) ? def.resY * (def.resX - 2) : 0;
    int bendV = (def.resY >= 3) ? (def.resY - 2) * def.resX : 0;
    mConstraints.Reserve(static_cast<unsigned int>(structuralH + structuralV + shear + bendH + bendV));

    for (int y = 0; y < def.resY; ++y)
    {
        for (int x = 0; x < def.resX; ++x)
        {
            Particle p;
            p.position = def.origin + Dia::Maths::Vector2D(
                static_cast<float>(x) * dx,
                -static_cast<float>(y) * dy);
            p.prevPosition = p.position;
            p.invMass = perParticleInvMass;
            p.radius = def.particleRadius;
            mParticles.Add(p);
            mOriginalInvMass.Add(perParticleInvMass);
        }
    }

    float diagLen = std::sqrt(dx * dx + dy * dy);

    // Structural — horizontal
    for (int y = 0; y < def.resY; ++y)
        for (int x = 0; x < def.resX - 1; ++x)
            AddConstraint(ParticleIndex(x, y), ParticleIndex(x + 1, y), dx, def.structuralStiffness, ConstraintType::kStructural);

    // Structural — vertical
    for (int y = 0; y < def.resY - 1; ++y)
        for (int x = 0; x < def.resX; ++x)
            AddConstraint(ParticleIndex(x, y), ParticleIndex(x, y + 1), dy, def.structuralStiffness, ConstraintType::kStructural);

    // Shear — diagonal down-right
    for (int y = 0; y < def.resY - 1; ++y)
        for (int x = 0; x < def.resX - 1; ++x)
            AddConstraint(ParticleIndex(x, y), ParticleIndex(x + 1, y + 1), diagLen, def.shearStiffness, ConstraintType::kShear);

    // Shear — diagonal down-left
    for (int y = 0; y < def.resY - 1; ++y)
        for (int x = 1; x < def.resX; ++x)
            AddConstraint(ParticleIndex(x, y), ParticleIndex(x - 1, y + 1), diagLen, def.shearStiffness, ConstraintType::kShear);

    // Bend — skip-one horizontal
    for (int y = 0; y < def.resY; ++y)
        for (int x = 0; x < def.resX - 2; ++x)
            AddConstraint(ParticleIndex(x, y), ParticleIndex(x + 2, y), 2.0f * dx, def.bendStiffness, ConstraintType::kBend);

    // Bend — skip-one vertical
    for (int y = 0; y < def.resY - 2; ++y)
        for (int x = 0; x < def.resX; ++x)
            AddConstraint(ParticleIndex(x, y), ParticleIndex(x, y + 2), 2.0f * dy, def.bendStiffness, ConstraintType::kBend);

    if (def.pinTopRow)
    {
        for (int x = 0; x < def.resX; ++x)
        {
            PinParticle(x, 0);
        }
    }
}

int Cloth::GetParticleCount() const
{
    return static_cast<int>(mParticles.Size());
}

Particle& Cloth::GetParticle(int x, int y)
{
    int idx = ParticleIndex(x, y);
    return mParticles[idx];
}

const Particle& Cloth::GetParticle(int x, int y) const
{
    int idx = ParticleIndex(x, y);
    return mParticles[idx];
}

int Cloth::GetConstraintCount() const
{
    return static_cast<int>(mConstraints.Size());
}

DistanceConstraint& Cloth::GetConstraint(int index)
{
    DIA_ASSERT(index >= 0 && index < static_cast<int>(mConstraints.Size()), "Constraint index out of range");
    return mConstraints[index];
}

const DistanceConstraint& Cloth::GetConstraint(int index) const
{
    DIA_ASSERT(index >= 0 && index < static_cast<int>(mConstraints.Size()), "Constraint index out of range");
    return mConstraints[index];
}

void Cloth::PinParticle(int x, int y)
{
    DIA_ASSERT(x >= 0 && x < mResX && y >= 0 && y < mResY, "Pin coords out of range");
    mParticles[ParticleIndex(x, y)].invMass = 0.0f;
}

void Cloth::UnpinParticle(int x, int y)
{
    DIA_ASSERT(x >= 0 && x < mResX && y >= 0 && y < mResY, "Unpin coords out of range");
    int idx = ParticleIndex(x, y);
    mParticles[idx].invMass = mOriginalInvMass[idx];
}

bool Cloth::IsTorn() const
{
    return mIsTorn;
}

const Dia::Core::StringCRC& Cloth::GetId() const
{
    return mId;
}

BodyType Cloth::GetBodyType() const
{
    return BodyType::kCloth;
}

int Cloth::GetResX() const
{
    return mResX;
}

int Cloth::GetResY() const
{
    return mResY;
}

float Cloth::GetMaxStretch() const
{
    return mMaxStretch;
}

void Cloth::CheckTearing()
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
#ifndef NDEBUG
            float stretchRatio = dist / c.restLength;
            DIA_LOG_DEBUG("Physics",
                "Cloth '%s': constraint [%d] torn at stretch ratio %.2f",
                mId.AsChar(), i, stretchRatio);
#endif
            c.active = false;
            mIsTorn = true;
        }
    }
}

int Cloth::ParticleIndex(int x, int y) const
{
    DIA_ASSERT(x >= 0 && x < mResX && y >= 0 && y < mResY, "Grid coords out of range");
    return y * mResX + x;
}

void Cloth::AddConstraint(int idxA, int idxB, float restLen, float stiffness, ConstraintType type)
{
    DistanceConstraint c;
    c.indexA = idxA;
    c.indexB = idxB;
    c.restLength = restLen;
    c.stiffness = stiffness;
    c.active = true;
    c.type = type;
    mConstraints.Add(c);
}

} // namespace Dia::SoftBody2D
