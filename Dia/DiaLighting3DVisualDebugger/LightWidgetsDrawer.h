////////////////////////////////////////////////////////////////////////////////
// Filename: LightWidgetsDrawer.h
// Description: IVisualDebugger that draws sphere/arrow widgets at each
//              registered light's position each frame. Point and spot lights
//              show a sphere (yellow); spot lights additionally show a
//              direction arrow; directional lights show a world-origin arrow
//              (cyan). Ambient lights produce no widget.
// Feature spec: docs/specs/applications/dia/systems/dialighting3dvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D { class LightRegistry3D; } }

namespace Dia { namespace Lighting3D {

////////////////////////////////////////////////////////////////////////////////
// LightWidgetsDrawer
//
// Registered under layer name LayerNames::kLightWidgets ("light3d.widgets").
// Each frame, iterates all enabled lights in LightRegistry3D and emits
// sphere/arrow primitives scaled by DebugLayerManager::GetDebugScale() * mWidgetScale.
////////////////////////////////////////////////////////////////////////////////
class LightWidgetsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit LightWidgetsDrawer(const LightRegistry3D& registry);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw    (Dia::Core::IDebugDraw& draw) override;

    void SetShowPointLights      (bool show)  { mShowPointLights       = show; }
    void SetShowSpotLights       (bool show)  { mShowSpotLights        = show; }
    void SetShowDirectionalLights(bool show)  { mShowDirectionalLights = show; }
    void SetDirLightAnchorHeight (float h)    { mDirLightAnchorHeight  = h; }

private:
    const LightRegistry3D& mRegistry;

    bool  mShowPointLights       = true;
    bool  mShowSpotLights        = true;
    bool  mShowDirectionalLights = true;
    float mWidgetScale           = 1.0f;
    float mDirLightAnchorHeight  = 3.0f;
};

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
