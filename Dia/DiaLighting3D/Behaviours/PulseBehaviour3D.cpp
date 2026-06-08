////////////////////////////////////////////////////////////////////////////////
// Filename: PulseBehaviour3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include <DiaLighting3D/Behaviours/PulseBehaviour3D.h>
#include <DiaLighting3D/Behaviours/LightBehaviourRegistry3D.h>
#include <cmath>

namespace Dia { namespace Lighting3D {

const Dia::Core::StringCRC PulseBehaviour3D::kTypeId("PulseBehaviour3D");

} }

namespace {
    struct PulseRegistrar
    {
        PulseRegistrar()
        {
            Dia::Lighting3D::LightBehaviourRegistry3D::Get().Register(
                Dia::Lighting3D::PulseBehaviour3D::kTypeId,
                [](const void*) -> Dia::Lighting3D::ILightBehaviour3D*
                {
                    return new Dia::Lighting3D::PulseBehaviour3D();
                }
            );
        }
    } sPulseRegistrar;
}

namespace Dia { namespace Lighting3D {

static const float kPi = 3.14159265358979323846f;

PulseBehaviour3D::PulseBehaviour3D()
    : mIntensity(nullptr)
    , mMin(0.5f)
    , mMax(1.0f)
    , mPeriod(2.0f)
    , mPhase(0.0f)
{
}

void PulseBehaviour3D::SetTarget(float* intensity)
{
    mIntensity = intensity;
}

void PulseBehaviour3D::SetRange(float min, float max)
{
    mMin = min;
    mMax = max;
}

void PulseBehaviour3D::SetPeriod(float seconds)
{
    mPeriod = seconds;
}

Dia::Core::StringCRC PulseBehaviour3D::GetTypeId() const
{
    return kTypeId;
}

void PulseBehaviour3D::Update(float dt)
{
    float twoPi = 2.0f * kPi;

    if (mPeriod > 0.0f)
    {
        mPhase += dt * (twoPi / mPeriod);
    }

    // Wrap to [0, 2*pi]
    while (mPhase >= twoPi)
    {
        mPhase -= twoPi;
    }

    float t = (sinf(mPhase) + 1.0f) * 0.5f;

    if (mIntensity != nullptr)
    {
        *mIntensity = mMin + t * (mMax - mMin);
    }
}

} }
