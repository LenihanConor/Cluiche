////////////////////////////////////////////////////////////////////////////////
// Filename: Camera2DModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "Modules/Camera2DModule.h"
#include "Modules/KernelModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC Camera2DModule::kTypeId("Camera2DModule");

Camera2DModule::Camera2DModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult Camera2DModule::DoStart()
{
    const unsigned int w = KernelModule::GetWindowWidth();
    const unsigned int h = KernelModule::GetWindowHeight();
    if (w > 0 && h > 0)
        mWindowSize = Dia::Maths::Vector2D(static_cast<float>(w), static_cast<float>(h));

    // Register the default camera and activate it
    mRegistry.Register(Dia::Core::StringCRC(kDefaultCameraId), Dia::Camera2D::Camera2D{});
    mRegistry.SetActive(Dia::Core::StringCRC(kDefaultCameraId));

    DIA_LOG_INFO("Application", "Camera2DModule::DoStart — window %.0fx%.0f", mWindowSize.x, mWindowSize.y);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void Camera2DModule::DoUpdate(float dt)
{
    // Sync window size from KernelModule's atomic (set once at startup, immutable after)
    const unsigned int w = KernelModule::GetWindowWidth();
    const unsigned int h = KernelModule::GetWindowHeight();
    if (w > 0 && h > 0)
        mWindowSize = Dia::Maths::Vector2D(static_cast<float>(w), static_cast<float>(h));

    mRegistry.UpdateAll(dt);
}

Dia::ApplicationFlow::StopResult Camera2DModule::DoStop()
{
    mRegistry.Unregister(Dia::Core::StringCRC(kDefaultCameraId));
    return Dia::ApplicationFlow::StopResult::kDone;
}

Dia::Camera2D::ViewportTransform Camera2DModule::GetViewportTransform() const
{
    return Dia::Camera2D::ViewportTransform(mRegistry.GetActive(), mWindowSize);
}

} } // namespace Cluiche::AppFlow

namespace { using Camera2DModule_ = Cluiche::AppFlow::Camera2DModule; }
DIA_MODULE(Camera2DModule_);
DIA_DESCRIBE(Camera2DModule_::kTypeId, "Owns the CameraRegistry2D for the SimPU; drives UpdateAll and viewport sync.");
