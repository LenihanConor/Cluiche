////////////////////////////////////////////////////////////////////////////////
// Filename: CameraModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "Modules/CameraModule.h"
#include "Modules/KernelModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC CameraModule::kTypeId("CameraModule");

CameraModule::CameraModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult CameraModule::DoStart()
{
    const unsigned int w = KernelModule::GetWindowWidth();
    const unsigned int h = KernelModule::GetWindowHeight();
    if (w > 0 && h > 0)
        mWindowSize = Dia::Maths::Vector2D(static_cast<float>(w), static_cast<float>(h));

    DIA_LOG_INFO("Application", "CameraModule::DoStart — window %.0fx%.0f", mWindowSize.x, mWindowSize.y);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void CameraModule::DoUpdate(float)
{
    // Sync window size from KernelModule's atomic (set once at startup, immutable after)
    const unsigned int w = KernelModule::GetWindowWidth();
    const unsigned int h = KernelModule::GetWindowHeight();
    if (w > 0 && h > 0)
        mWindowSize = Dia::Maths::Vector2D(static_cast<float>(w), static_cast<float>(h));
}

Dia::ApplicationFlow::StopResult CameraModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

Dia::Graphics::ViewportTransform CameraModule::GetViewportTransform() const
{
    return Dia::Graphics::ViewportTransform(mCamera, mWindowSize);
}

} } // namespace Cluiche::AppFlow

namespace { using CameraModule_ = Cluiche::AppFlow::CameraModule; }
DIA_MODULE(CameraModule_);
