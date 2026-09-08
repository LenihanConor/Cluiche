////////////////////////////////////////////////////////////////////////////////
// Filename: FlickerBehaviour3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaLighting3D/Behaviours/ILightBehaviour3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D {

class FlickerBehaviour3D : public ILightBehaviour3D
{
public:
    static const Dia::Core::StringCRC kTypeId;

    FlickerBehaviour3D();

    void SetTarget   (float* intensity);
    void SetRange    (float min, float max);
    void SetFrequency(float hz);

    Dia::Core::StringCRC GetTypeId() const override;
    void                 Update(float dt) override;

private:
    float*       mIntensity  = nullptr;
    float        mMin        = 0.8f;
    float        mMax        = 1.0f;
    float        mFrequency  = 10.0f;  // hz
    float        mTimer      = 0.0f;
    float        mCurrent    = 1.0f;
    float        mTarget     = 1.0f;
    unsigned int mSeed       = 12345u;
};

} }
