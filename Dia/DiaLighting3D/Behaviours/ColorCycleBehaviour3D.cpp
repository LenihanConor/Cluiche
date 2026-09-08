////////////////////////////////////////////////////////////////////////////////
// Filename: ColorCycleBehaviour3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include <DiaLighting3D/Behaviours/ColorCycleBehaviour3D.h>
#include <DiaLighting3D/Behaviours/LightBehaviourRegistry3D.h>

namespace Dia { namespace Lighting3D {

const Dia::Core::StringCRC ColorCycleBehaviour3D::kTypeId("ColorCycleBehaviour3D");

} }

namespace {
    struct ColorCycleRegistrar
    {
        ColorCycleRegistrar()
        {
            Dia::Lighting3D::LightBehaviourRegistry3D::Get().Register(
                Dia::Lighting3D::ColorCycleBehaviour3D::kTypeId,
                [](const void*) -> Dia::Lighting3D::ILightBehaviour3D*
                {
                    return new Dia::Lighting3D::ColorCycleBehaviour3D();
                }
            );
        }
    } sColorCycleRegistrar;
}

namespace Dia { namespace Lighting3D {

ColorCycleBehaviour3D::ColorCycleBehaviour3D()
    : mColour(nullptr)
    , mPaletteCount(0)
    , mPeriod(3.0f)
    , mPhase(0.0f)
{
}

void ColorCycleBehaviour3D::SetTarget(Dia::Core::RGBA* colour)
{
    mColour = colour;
}

void ColorCycleBehaviour3D::SetPalette(const Dia::Core::RGBA* colours, unsigned int count)
{
    if (count > kMaxPaletteSize)
    {
        count = kMaxPaletteSize;
    }
    for (unsigned int i = 0; i < count; ++i)
    {
        mPalette[i] = colours[i];
    }
    mPaletteCount = count;
}

void ColorCycleBehaviour3D::SetPeriod(float seconds)
{
    mPeriod = seconds;
}

Dia::Core::StringCRC ColorCycleBehaviour3D::GetTypeId() const
{
    return kTypeId;
}

void ColorCycleBehaviour3D::Update(float dt)
{
    if (mPaletteCount < 2 || mColour == nullptr)
    {
        return;
    }

    if (mPeriod > 0.0f)
    {
        mPhase += dt / mPeriod;
    }

    // Wrap to [0, 1)
    while (mPhase >= 1.0f)
    {
        mPhase -= 1.0f;
    }

    float        pos  = mPhase * static_cast<float>(mPaletteCount);
    unsigned int idx  = static_cast<unsigned int>(pos);
    float        t    = pos - static_cast<float>(idx);
    unsigned int next = (idx + 1) % mPaletteCount;

    // Lerp each channel using signed arithmetic to handle direction correctly
    int r = static_cast<int>(mPalette[idx].R()) + static_cast<int>(t * static_cast<float>(static_cast<int>(mPalette[next].R()) - static_cast<int>(mPalette[idx].R())));
    int g = static_cast<int>(mPalette[idx].G()) + static_cast<int>(t * static_cast<float>(static_cast<int>(mPalette[next].G()) - static_cast<int>(mPalette[idx].G())));
    int b = static_cast<int>(mPalette[idx].B()) + static_cast<int>(t * static_cast<float>(static_cast<int>(mPalette[next].B()) - static_cast<int>(mPalette[idx].B())));
    int a = static_cast<int>(mPalette[idx].A()) + static_cast<int>(t * static_cast<float>(static_cast<int>(mPalette[next].A()) - static_cast<int>(mPalette[idx].A())));

    // Clamp to [0, 255]
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    if (a < 0) a = 0; if (a > 255) a = 255;

    *mColour = Dia::Core::RGBA(
        static_cast<unsigned char>(r),
        static_cast<unsigned char>(g),
        static_cast<unsigned char>(b),
        static_cast<unsigned char>(a)
    );
}

} }
