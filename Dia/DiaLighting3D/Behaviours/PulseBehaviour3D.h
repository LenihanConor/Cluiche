////////////////////////////////////////////////////////////////////////////////
// Filename: PulseBehaviour3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaLighting3D/Behaviours/ILightBehaviour3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D {

class PulseBehaviour3D : public ILightBehaviour3D
{
public:
    static const Dia::Core::StringCRC kTypeId;

    PulseBehaviour3D();

    void SetTarget(float* intensity);
    void SetRange (float min, float max);
    void SetPeriod(float seconds);

    Dia::Core::StringCRC GetTypeId() const override;
    void                 Update(float dt) override;

private:
    float* mIntensity = nullptr;
    float  mMin       = 0.5f;
    float  mMax       = 1.0f;
    float  mPeriod    = 2.0f;  // seconds
    float  mPhase     = 0.0f;  // 0..2pi
};

} }
