#pragma once
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/Strings/String64.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/EventData.h>
#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaSDL/WindowFactory.h>
#include <DiaSDL/Window.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include "Types/MainToSimEvent.h"
#include "Modules/JobSystemModule.h"

namespace Dia { namespace Bgfx3D { class Canvas3D; } }

#ifdef DIA_DEBUG
namespace Dia { namespace Bgfx { class BgfxImGuiBackend; } }
#endif

#include <atomic>

namespace Dia { namespace Observation { namespace Metric {
    class Gauge;
    class Histogram;
} } }

namespace Dia { namespace Graphics { class ICanvas; } }
namespace Dia { namespace Window { class IWindow; } }

namespace Cluiche { namespace AppFlow {

class KernelModule : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Window creation, canvas, and texture handler";
    explicit KernelModule(const Dia::Core::StringCRC& instanceId);

    Dia::Graphics::ICanvas* GetCanvas() { return mCanvas; }
    Dia::Window::IWindow*   GetWindow() { return mWindow; }

    const Dia::Input::EventData& GetFrameInputEvents() const { return mFrameEvents; }

    static void SetRenderContextReleased(bool released) { sRenderContextReleased.store(released, std::memory_order_release); }
    static bool IsRenderContextReleased()               { return sRenderContextReleased.load(std::memory_order_acquire); }

    static void SetRenderContextActive(bool active) { sRenderContextActive.store(active, std::memory_order_release); }
    static bool IsRenderContextActive()             { return sRenderContextActive.load(std::memory_order_acquire); }

    // Window size — set once at startup from .diagame, safe to read cross-PU (immutable after DoStart)
    static unsigned int GetWindowWidth()  { return sWindowWidth.load(std::memory_order_acquire); }
    static unsigned int GetWindowHeight() { return sWindowHeight.load(std::memory_order_acquire); }

protected:
    void OnConfigure(const char* configJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::EventStreamWriter<MainToSimEvent>                      mInputWriter{this, "MainToSim"};
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::Graphics::ICanvas>           mCanvasService{this, "KernelCanvas"};
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::AssetRuntime::TextureHandler> mTextureHandlerService{this, "KernelTextureHandler"};
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::Mesh3D::Mesh3DAssetHandler>  mMeshHandlerService{this, "KernelMeshHandler"};
    static std::atomic<bool>         sRenderContextReleased;
    static std::atomic<bool>         sRenderContextActive;
    static std::atomic<unsigned int> sWindowWidth;
    static std::atomic<unsigned int> sWindowHeight;

    // Window config — populated from .diagame in DoStart, with fallback defaults
    Dia::Core::Containers::String64 mWindowTitle{"CluicheTest"};
    unsigned int                    mWindowWidth  = 1400;
    unsigned int                    mWindowHeight = 1000;

    Dia::Input::InputSourceManager  mInputSourceManager;
    Dia::Input::ConsoleGamepadManager mGamepadManager;
    Dia::SDL::WindowFactory         mWindowFactory;
    Dia::Window::IWindow*           mWindow       = nullptr;
    Dia::Graphics::ICanvas*         mCanvas       = nullptr;
    Dia::Bgfx3D::Canvas3D*          mBgfxCanvas   = nullptr;
    Dia::AssetRuntime::TextureHandler mTextureHandler;
    Dia::Mesh3D::Mesh3DAssetHandler  mMeshHandler;
    Dia::Input::EventData           mFrameEvents;

#ifdef DIA_DEBUG
    Dia::Bgfx::BgfxImGuiBackend* mBgfxImGuiBackend = nullptr;
#endif

    Dia::ApplicationFlow::ModuleRef<JobSystemModule> mJobSystemRef{this};
    Dia::Observation::Metric::Gauge*     mMetricInputSources    = nullptr;
    Dia::Observation::Metric::Histogram* mMetricEventsPerFrame  = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricActiveGamepads  = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricMeshDrawCalls   = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricGpuMeshCount    = nullptr;
};

} } // namespace Cluiche::AppFlow
