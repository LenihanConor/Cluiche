////////////////////////////////////////////////////////////////////////////////
// Filename: LightRangesDrawer.h
// Description: IVisualDebugger that draws influence-range spheres around each
//              registered point and spot light each frame.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D { class LightRegistry3D; } }

namespace Dia { namespace Lighting3D {

class LightRangesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit LightRangesDrawer(const LightRegistry3D& registry);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const LightRegistry3D& mRegistry;
};

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
