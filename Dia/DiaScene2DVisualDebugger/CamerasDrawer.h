////////////////////////////////////////////////////////////////////////////////
// Filename: CamerasDrawer.h
// Description: IVisualDebugger that draws registered Camera2D positions.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::Camera2D  { class CameraRegistry2D; }
namespace Dia::Core      { class IDebugContext; }

namespace Dia::Scene2DVisualDebugger
{

class CamerasDrawer : public Dia::Debug::IVisualDebugger
{
public:
    CamerasDrawer(const Dia::Camera2D::CameraRegistry2D& cameraRegistry,
                  const Dia::Core::IDebugContext&        manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Dia::Camera2D::CameraRegistry2D& mCameraRegistry;
    const Dia::Core::IDebugContext&        mManager;
};

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
