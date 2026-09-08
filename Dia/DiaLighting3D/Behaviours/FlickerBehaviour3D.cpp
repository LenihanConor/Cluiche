////////////////////////////////////////////////////////////////////////////////
// Filename: FlickerBehaviour3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include <DiaLighting3D/Behaviours/FlickerBehaviour3D.h>
#include <DiaLighting3D/Behaviours/LightBehaviourRegistry3D.h>

namespace Dia { namespace Lighting3D {

const Dia::Core::StringCRC FlickerBehaviour3D::kTypeId("FlickerBehaviour3D");

} }

namespace {
    struct FlickerRegistrar
    {
        FlickerRegistrar()
        {
            Dia::Lighting3D::LightBehaviourRegistry3D::Get().Register(
                Dia::Lighting3D::FlickerBehaviour3D::kTypeId,
                [](const void*) -> Dia::Lighting3D::ILightBehaviour3D*
                {
                    return new Dia::Lighting3D::FlickerBehaviour3D();
                }
            );
        }
    } sFlickerRegistrar;
}

namespace Dia { namespace Lighting3D {

FlickerBehaviour3D::FlickerBehaviour3D()
    : mIntensity(nullptr)
    , mMin(0.8f)
    , mMax(1.0f)
    , mFrequency(10.0f)
    , mTimer(0.0f)
    , mCurrent(1.0f)
    , mTarget(1.0f)
    , mSeed(12345u)
{
}

void FlickerBehaviour3D::SetTarget(float* intensity)
{
    mIntensity = intensity;
}

void FlickerBehaviour3D::SetRange(float min, float max)
{
    mMin = min;
    mMax = max;
}

void FlickerBehaviour3D::SetFrequency(float hz)
{
    mFrequency = hz;
}

Dia::Core::StringCRC FlickerBehaviour3D::GetTypeId() const
{
    return kTypeId;
}

void FlickerBehaviour3D::Update(float dt)
{
    mTimer += dt;

    float interval = (mFrequency > 0.0f) ? (1.0f / mFrequency) : 0.1f;
    if (mTimer >= interval)
    {
        // LCG random in [min, max]
        mSeed = mSeed * 1664525u + 1013904223u;
        float r = static_cast<float>(mSeed >> 16) / 65535.0f;
        mTarget = mMin + r * (mMax - mMin);
        mTimer = 0.0f;
    }

    // Lerp current toward target
    float lerpRate = mFrequency * dt * 5.0f;
    if (lerpRate > 1.0f) lerpRate = 1.0f;
    mCurrent = mCurrent + lerpRate * (mTarget - mCurrent);

    if (mIntensity != nullptr)
    {
        *mIntensity = mCurrent;
    }
}

} }
