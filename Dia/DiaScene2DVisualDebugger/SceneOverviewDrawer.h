////////////////////////////////////////////////////////////////////////////////
// Filename: SceneOverviewDrawer.h
// Description: IVisualDebugger that draws an overview of a Scene2D's cameras,
//              lights, and layer bands.
// Feature spec: docs/specs/features/dia/diavisualdebugger/drawer-boundary-consistency.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Camera2D  { class CameraRegistry2D; }
namespace Dia::Lighting2D { class LightRegistry2D; }
namespace Dia::Scene2D   { class LayerTable; }
namespace Dia::Debug     { class DebugLayerManager; }

namespace Dia::Scene2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// SceneOverviewDrawer
//
// Renders per-frame:
//   - World bounds rect outline
//   - Layer bands (horizontal strips coloured by depth order)
//   - Active camera position (circle) + FOV rect
//   - Each registered light (circle at position + radius ring)
//
// Does NOT render entities — that belongs in a future DiaEntityVisualDebugger.
//
// Layer: LayerNames::kScene2DOverview   Priority: 5
////////////////////////////////////////////////////////////////////////////////
class SceneOverviewDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SceneOverviewDrawer(
        const Dia::Camera2D::CameraRegistry2D&  cameraRegistry,
        const Dia::Lighting2D::LightRegistry2D& lightRegistry,
        const Dia::Scene2D::LayerTable&         layerTable,
        const Dia::Debug::DebugLayerManager&    manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const Dia::Camera2D::CameraRegistry2D&  mCameraRegistry;
    const Dia::Lighting2D::LightRegistry2D& mLightRegistry;
    const Dia::Scene2D::LayerTable&         mLayerTable;
    const Dia::Debug::DebugLayerManager&    mManager;
};

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
