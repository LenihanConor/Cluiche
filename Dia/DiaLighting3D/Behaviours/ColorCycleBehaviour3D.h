////////////////////////////////////////////////////////////////////////////////
// Filename: ColorCycleBehaviour3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaLighting3D/Behaviours/ILightBehaviour3D.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D {

class ColorCycleBehaviour3D : public ILightBehaviour3D
{
public:
    static constexpr unsigned int kMaxPaletteSize = 8;
    static const Dia::Core::StringCRC kTypeId;

    ColorCycleBehaviour3D();

    void SetTarget (Dia::Core::RGBA* colour);
    void SetPalette(const Dia::Core::RGBA* colours, unsigned int count);
    void SetPeriod (float seconds);

    Dia::Core::StringCRC GetTypeId() const override;
    void                 Update(float dt) override;

private:
    Dia::Core::RGBA* mColour       = nullptr;
    Dia::Core::RGBA  mPalette[kMaxPaletteSize];
    unsigned int     mPaletteCount = 0;
    float            mPeriod       = 3.0f;
    float            mPhase        = 0.0f;  // 0..1 across full palette cycle
};

} }
