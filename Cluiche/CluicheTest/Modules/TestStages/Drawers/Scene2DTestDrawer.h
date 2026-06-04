#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <diaentitytemplate/Domain.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaScene2D/LayerTable.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace CluicheTest {

class Scene2DTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    Scene2DTestDrawer(
        const Dia::Entity::Domain&             domain,
        const Dia::Camera2D::CameraRegistry2D& cameraRegistry,
        const Dia::Lighting2D::LightRegistry2D& lightRegistry,
        const Dia::Scene2D::LayerTable&        layerTable,
        const Dia::Debug::DebugLayerManager&   mgr);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    const Dia::Entity::Domain&              mDomain;
    const Dia::Camera2D::CameraRegistry2D&  mCameraRegistry;
    const Dia::Lighting2D::LightRegistry2D& mLightRegistry;
    const Dia::Scene2D::LayerTable&         mLayerTable;
    const Dia::Debug::DebugLayerManager&    mManager;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
