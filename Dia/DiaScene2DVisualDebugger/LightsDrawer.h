////////////////////////////////////////////////////////////////////////////////
// Filename: LightsDrawer.h
// Description: IVisualDebugger that draws registered Lighting2D light positions
//              and influence radii.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::Lighting2D { class LightRegistry2D; }
namespace Dia::Core       { class IDebugContext; }

namespace Dia::Scene2DVisualDebugger
{

class LightsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    LightsDrawer(const Dia::Lighting2D::LightRegistry2D& lightRegistry,
                 const Dia::Core::IDebugContext&         manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::Lighting2D::LightRegistry2D& mLightRegistry;
    const Dia::Core::IDebugContext&         mManager;
};

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
