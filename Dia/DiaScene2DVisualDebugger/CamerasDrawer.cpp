////////////////////////////////////////////////////////////////////////////////
// Filename: CamerasDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaScene2DVisualDebugger/CamerasDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaCamera2D/Camera2D.h>
#include <DiaGeometry2DVisualDebugger/ShapeDrawer.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>

namespace Dia::Scene2DVisualDebugger
{

static const Dia::Graphics::RGBA kCameraColour(130, 130, 240, 255);

CamerasDrawer::CamerasDrawer(const Dia::Camera2D::CameraRegistry2D& cameraRegistry,
                             const Dia::Core::IDebugContext&        manager)
    : mCameraRegistry(cameraRegistry)
    , mManager(manager)
{}

Dia::Core::StringCRC CamerasDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kScene2DCameras;
}

void CamerasDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;
    if (mCameraRegistry.GetCount() == 0) return;

    Dia::Geometry2DVisualDebugger::ShapeDrawer drawer(mManager);
    const auto& cam = mCameraRegistry.GetActive();
    Dia::Geometry2D::Circle camCircle(20.0f, cam.GetPosition());
    drawer.SubmitCircle(camCircle, kCameraColour);
    drawer.Draw(draw);
}

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
