////////////////////////////////////////////////////////////////////////////////
// Filename: CameraModule.h
// Description: Owns the active 2D camera and window size for the SimPU.
//              Non-debug, all stages. VisualDebuggerModule and PickingModule
//              both read from this via ModuleRef.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Cluiche { namespace AppFlow {

class CameraModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Owns the active 2D camera and window size";

    explicit CameraModule(const Dia::Core::StringCRC& instanceId);

    const Dia::Graphics::Camera2D&    GetCamera()          const { return mCamera; }
    const Dia::Maths::Vector2D&       GetWindowSize()      const { return mWindowSize; }
    Dia::Graphics::ViewportTransform  GetViewportTransform() const;

    void SetCamera(const Dia::Graphics::Camera2D& cam)       { mCamera = cam; }
    void SetWindowSize(const Dia::Maths::Vector2D& size)     { mWindowSize = size; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()            override;
    void                              DoUpdate(float)      override;
    Dia::ApplicationFlow::StopResult  DoStop()             override;

private:
    Dia::Graphics::Camera2D    mCamera;
    Dia::Maths::Vector2D       mWindowSize{ 1400.0f, 1000.0f };
};

} } // namespace Cluiche::AppFlow
