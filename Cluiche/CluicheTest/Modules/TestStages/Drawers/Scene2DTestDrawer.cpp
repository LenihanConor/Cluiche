#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Scene2DTestDrawer.h"
#include "Modules/TestStages/Entity/TransformComponent.h"

#include <DiaScene2DVisualDebugger/SceneOverviewDrawer.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <imgui.h>

namespace CluicheTest {

static const Dia::Graphics::RGBA kEntityColour(100, 200, 120, 255);

Scene2DTestDrawer::Scene2DTestDrawer(
    const Dia::Entity::Domain&             domain,
    const Dia::Camera2D::CameraRegistry2D& cameraRegistry,
    const Dia::Lighting2D::LightRegistry2D& lightRegistry,
    const Dia::Scene2D::LayerTable&        layerTable,
    const Dia::Debug::DebugLayerManager&   mgr)
    : mDomain(domain)
    , mCameraRegistry(cameraRegistry)
    , mLightRegistry(lightRegistry)
    , mLayerTable(layerTable)
    , mManager(mgr)
{}

Dia::Core::StringCRC Scene2DTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("scene2d.overview");
}

#pragma warning(push)
#pragma warning(disable: 6262)
void Scene2DTestDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    // Engine-side: cameras, lights, layer bands
    Dia::Scene2DVisualDebugger::SceneOverviewDrawer overview(
        mCameraRegistry, mLightRegistry, mLayerTable, mManager);
    overview.Draw(frameData);

    // Test-specific: entities via TransformComponent (CluicheTest-only)
    Dia::Geometry2DVisualDebugger::ShapeDrawer entityDrawer(mManager);
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        Dia::Entity::Entity e = mDomain.GetAliveEntity(i);
        if (!e.IsValid()) continue;

        auto* tc = mDomain.GetComponent<TransformComponent>(e);
        if (!tc) continue;

        float vx = (tc->x / 1920.0f) * 800.0f - 400.0f;
        float vy = (tc->y / 1080.0f) * 600.0f - 300.0f;
        Dia::Geometry2D::Circle entityCircle(18.0f, Dia::Maths::Vector2D(vx, vy));
        entityDrawer.SubmitCircle(entityCircle, kEntityColour);
    }
    entityDrawer.Draw(frameData);
}
#pragma warning(pop)

void Scene2DTestDrawer::DrawImGui()
{
    // Engine-side ImGui delegated to SceneOverviewDrawer when registered directly;
    // here we just show the entity count as test-specific info
    ImGui::Text("Entities: %u", mDomain.GetEntityCount());
}

} // namespace CluicheTest

#endif // DIA_DEBUG
