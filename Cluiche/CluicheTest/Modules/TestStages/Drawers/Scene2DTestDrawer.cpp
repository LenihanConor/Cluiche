#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/Scene2DTestDrawer.h"
#include "Modules/TestStages/Entity/TransformComponent.h"

#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCamera2D/Camera2D.h>
#include <DiaLighting2D/PointLight2D.h>
#include <imgui.h>

namespace CluicheTest {

static const Dia::Graphics::RGBA kEntityColour(100, 200, 120, 255);
static const Dia::Graphics::RGBA kCameraColour(130, 130, 240, 255);
static const Dia::Graphics::RGBA kLightWarmColour(255, 180, 60, 255);
static const Dia::Graphics::RGBA kLightCoolColour(60, 160, 255, 255);
static const Dia::Graphics::RGBA kLayerBgColour(60, 60, 100, 60);
static const Dia::Graphics::RGBA kLayerMidColour(80, 80, 120, 50);
static const Dia::Graphics::RGBA kLayerFgColour(100, 100, 140, 40);
static const Dia::Graphics::RGBA kWorldBoundsColour(80, 80, 140, 120);

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

void Scene2DTestDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);

    // Draw world bounds as a rect outline
    Dia::Geometry2D::AARect worldBounds(
        Dia::Maths::Vector2D(-400.0f, -300.0f),
        Dia::Maths::Vector2D(400.0f, 300.0f));
    drawer.SubmitAARect(worldBounds, kWorldBoundsColour);

    // Draw layer bands (horizontal strips)
    const unsigned int layerCount = mLayerTable.GetCount();
    if (layerCount > 0)
    {
        const float totalHeight = 600.0f;
        const float bandHeight = totalHeight / static_cast<float>(layerCount);
        const float startY = -300.0f;

        for (unsigned int i = 0; i < layerCount; ++i)
        {
            float y0 = startY + bandHeight * static_cast<float>(i);
            float y1 = y0 + bandHeight;
            Dia::Geometry2D::AARect band(
                Dia::Maths::Vector2D(-400.0f, y0),
                Dia::Maths::Vector2D(400.0f, y1));

            Dia::Graphics::RGBA colour = (i == 0) ? kLayerBgColour : (i == 1) ? kLayerMidColour : kLayerFgColour;
            drawer.SubmitAARect(band, colour);
        }
    }

    // Draw camera as a circle
    if (mCameraRegistry.GetCount() > 0)
    {
        const auto& cam = mCameraRegistry.GetActive();
        Dia::Geometry2D::Circle camCircle(20.0f, cam.GetPosition());
        drawer.SubmitCircle(camCircle, kCameraColour);

        // FOV indicator rect
        Dia::Geometry2D::AARect fov(
            Dia::Maths::Vector2D(cam.GetPosition().x - 60.0f, cam.GetPosition().y - 40.0f),
            Dia::Maths::Vector2D(cam.GetPosition().x + 60.0f, cam.GetPosition().y + 40.0f));
        drawer.SubmitAARect(fov, Dia::Graphics::RGBA(130, 130, 240, 80));
    }

    // Draw lights as circles with radius indicator
    for (unsigned int i = 0; i < mLightRegistry.GetCount(); ++i)
    {
        const auto& light = mLightRegistry.GetByIndex(i);
        if (!light.enabled) continue;

        Dia::Graphics::RGBA colour = (i == 0) ? kLightWarmColour : kLightCoolColour;
        Dia::Geometry2D::Circle lightCircle(12.0f, light.position);
        drawer.SubmitCircle(lightCircle, colour);

        Dia::Geometry2D::Circle radiusCircle(light.radius > 0.0f ? light.radius : 50.0f, light.position);
        Dia::Graphics::RGBA radiusColour(colour.R(), colour.G(), colour.B(), 40);
        drawer.SubmitCircle(radiusCircle, radiusColour);
    }

    // Draw entities as circles at their TransformComponent positions
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        Dia::Entity::Entity e = mDomain.GetAliveEntity(i);
        if (!e.IsValid()) continue;

        auto* tc = mDomain.GetComponent<TransformComponent>(e);
        if (!tc) continue;

        // Scale positions from world space [0,1920]x[0,1080] to view space [-400,400]x[-300,300]
        float vx = (tc->x / 1920.0f) * 800.0f - 400.0f;
        float vy = (tc->y / 1080.0f) * 600.0f - 300.0f;

        Dia::Geometry2D::Circle entityCircle(18.0f, Dia::Maths::Vector2D(vx, vy));
        drawer.SubmitCircle(entityCircle, kEntityColour);
    }

    drawer.Draw(frameData);
}

void Scene2DTestDrawer::DrawImGui()
{
    ImGui::Text("Scene2D Overview");
    ImGui::Separator();
    ImGui::Text("Layers: %u", mLayerTable.GetCount());
    for (unsigned int i = 0; i < mLayerTable.GetCount(); ++i)
    {
        const auto& layer = mLayerTable.GetByIndex(i);
        ImGui::BulletText("bit %u  sort:%d  parallax:(%.1f,%.1f)",
            i, layer.sortOrder, layer.parallax.x, layer.parallax.y);
    }
    ImGui::Separator();
    ImGui::Text("Cameras: %u", mCameraRegistry.GetCount());
    if (mCameraRegistry.GetCount() > 0)
    {
        const auto& cam = mCameraRegistry.GetActive();
        ImGui::BulletText("active @ (%.0f, %.0f)", cam.GetPosition().x, cam.GetPosition().y);
    }
    ImGui::Separator();
    ImGui::Text("Lights: %u", mLightRegistry.GetCount());
    for (unsigned int i = 0; i < mLightRegistry.GetCount(); ++i)
    {
        const auto& light = mLightRegistry.GetByIndex(i);
        ImGui::BulletText("%s @ (%.0f, %.0f) mask:0x%X",
            light.enabled ? "ON" : "OFF", light.position.x, light.position.y, light.layerMask);
    }
    ImGui::Separator();
    ImGui::Text("Entities: %u", mDomain.GetEntityCount());
}

} // namespace CluicheTest

#endif // DIA_DEBUG
