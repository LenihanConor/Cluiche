////////////////////////////////////////////////////////////////////////////////
// Filename: LightPathBehaviour3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include <DiaLighting3D/Behaviours/LightPathBehaviour3D.h>
#include <DiaLighting3D/Behaviours/LightBehaviourRegistry3D.h>

namespace Dia { namespace Lighting3D {

const Dia::Core::StringCRC LightPathBehaviour3D::kTypeId("LightPathBehaviour3D");

} }

namespace {
    struct LightPathRegistrar
    {
        LightPathRegistrar()
        {
            Dia::Lighting3D::LightBehaviourRegistry3D::Get().Register(
                Dia::Lighting3D::LightPathBehaviour3D::kTypeId,
                [](const void*) -> Dia::Lighting3D::ILightBehaviour3D*
                {
                    return nullptr;  // factory-created instances require a light pointer — use constructors directly
                }
            );
        }
    } sLightPathRegistrar;
}

namespace Dia { namespace Lighting3D {

LightPathBehaviour3D::LightPathBehaviour3D(PointLight3D* light, const Config& config)
    : mLightType(LightType::Point)
    , mPointLight(light)
    , mSpotLight(nullptr)
    , mSpline(config.spline)
    , mSpeed(config.speed)
    , mLoopMode(config.loopMode)
    , mT(0.0f)
    , mDirection(1.0f)
{
}

LightPathBehaviour3D::LightPathBehaviour3D(SpotLight3D* light, const Config& config)
    : mLightType(LightType::Spot)
    , mPointLight(nullptr)
    , mSpotLight(light)
    , mSpline(config.spline)
    , mSpeed(config.speed)
    , mLoopMode(config.loopMode)
    , mT(0.0f)
    , mDirection(1.0f)
{
}

Dia::Core::StringCRC LightPathBehaviour3D::GetTypeId() const
{
    return kTypeId;
}

void LightPathBehaviour3D::Update(float dt)
{
    mT += mDirection * mSpeed * dt;

    switch (mLoopMode)
    {
        case LoopMode::Loop:
        {
            if (mT > 1.0f)
                mT -= 1.0f;
            else if (mT < 0.0f)
                mT += 1.0f;
            break;
        }
        case LoopMode::PingPong:
        {
            if (mT >= 1.0f)
            {
                mT = 1.0f;
                mDirection = -1.0f;
            }
            else if (mT <= 0.0f)
            {
                mT = 0.0f;
                mDirection = 1.0f;
            }
            break;
        }
        case LoopMode::Once:
        {
            if (mT > 1.0f)
                mT = 1.0f;
            else if (mT < 0.0f)
                mT = 0.0f;
            break;
        }
    }

    const Dia::Maths::Vector3D pos = mSpline.Evaluate(mT);

    if (mLightType == LightType::Point && mPointLight != nullptr)
    {
        mPointLight->position = pos;
    }
    else if (mLightType == LightType::Spot && mSpotLight != nullptr)
    {
        mSpotLight->position = pos;
    }
}

float LightPathBehaviour3D::GetT() const
{
    return mT;
}

LightPathBehaviour3D::LoopMode LightPathBehaviour3D::GetLoopMode() const
{
    return mLoopMode;
}

} }
