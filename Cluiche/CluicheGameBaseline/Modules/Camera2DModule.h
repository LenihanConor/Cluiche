////////////////////////////////////////////////////////////////////////////////
// Filename: Camera2DModule.h
// Description: Owns the CameraRegistry2D for the SimPU.
//              Registers a "default" camera on start. Other code uses
//              GetRegistry() for multi-camera access or GetActiveCamera() /
//              GetViewportTransform() for the common single-camera case.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCamera2D/Camera2D.h>
#include <DiaCamera2D/ViewportTransform.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Cluiche { namespace AppFlow {

class Camera2DModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Owns the CameraRegistry2D for the SimPU";

    static constexpr const char* kDefaultCameraId = "default";

    explicit Camera2DModule(const Dia::Core::StringCRC& instanceId);

    // Registry access (full multi-camera API)
    Dia::Camera2D::CameraRegistry2D& GetRegistry() { return mRegistry; }
    const Dia::Camera2D::CameraRegistry2D& GetRegistry() const { return mRegistry; }

    // Convenience: active camera (single-camera common case)
    bool                           HasActiveCamera()   const { return mRegistry.HasActive(); }
    const Dia::Camera2D::Camera2D& GetActiveCamera()  const { return mRegistry.GetActive(); }
    Dia::Camera2D::Camera2D&       GetActiveCamera()        { return mRegistry.GetActive(); }

    // Delegates to active camera
    const Dia::Camera2D::Camera2D& GetCamera()    const { return GetActiveCamera(); }
    void SetCamera(const Dia::Camera2D::Camera2D& cam)  { GetActiveCamera() = cam; }

    const Dia::Maths::Vector2D& GetWindowSize()      const { return mWindowSize; }
    void SetWindowSize(const Dia::Maths::Vector2D& size)   { mWindowSize = size; }

    Dia::Camera2D::ViewportTransform GetViewportTransform() const;

protected:
    Dia::ApplicationFlow::StartResult DoStart()            override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop()             override;

private:
    Dia::Camera2D::CameraRegistry2D mRegistry;
    Dia::Maths::Vector2D            mWindowSize{ 1400.0f, 1000.0f };
};

} } // namespace Cluiche::AppFlow
