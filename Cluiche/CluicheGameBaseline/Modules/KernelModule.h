#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaApplicationFlow/Streams/ServiceStreamWriter.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/EventData.h>
#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaSFML/WindowFactory.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include "Types/MainToSimEvent.h"

namespace Dia { namespace Bgfx { class Canvas; } }

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

class KernelModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit KernelModule(const Dia::Core::StringCRC& instanceId);

    Dia::Graphics::ICanvas* GetCanvas() { return mCanvas; }
    Dia::Window::IWindow*   GetWindow() { return mWindow; }

    const Dia::Input::EventData& GetFrameInputEvents() const { return mFrameEvents; }

    static void SetRenderContextReleased(bool released) { sRenderContextReleased.store(released, std::memory_order_release); }
    static bool IsRenderContextReleased()               { return sRenderContextReleased.load(std::memory_order_acquire); }

    static void SetRenderContextActive(bool active) { sRenderContextActive.store(active, std::memory_order_release); }
    static bool IsRenderContextActive()             { return sRenderContextActive.load(std::memory_order_acquire); }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::EventStreamWriter<MainToSimEvent>                      mInputWriter{this, "MainToSim"};
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::Graphics::ICanvas>           mCanvasService{this, "KernelCanvas"};
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::AssetRuntime::TextureHandler> mTextureHandlerService{this, "KernelTextureHandler"};
    static std::atomic<bool>                                                     sRenderContextReleased;
    static std::atomic<bool>                                                     sRenderContextActive;

    Dia::Input::InputSourceManager  mInputSourceManager;
    Dia::Input::ConsoleGamepadManager mGamepadManager;
    Dia::SFML::WindowFactory        mWindowFactory;
    Dia::Window::IWindow*           mWindow       = nullptr;
    Dia::Graphics::ICanvas*         mCanvas       = nullptr;
    Dia::Bgfx::Canvas*              mBgfxCanvas   = nullptr;
    Dia::AssetRuntime::TextureHandler mTextureHandler;
    Dia::Input::EventData           mFrameEvents;

#ifdef DIA_DEBUG
    Dia::Bgfx::BgfxImGuiBackend* mBgfxImGuiBackend = nullptr;
#endif

    Dia::Observation::Metric::Gauge*     mMetricInputSources    = nullptr;
    Dia::Observation::Metric::Histogram* mMetricEventsPerFrame  = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricActiveGamepads  = nullptr;
};

} } // namespace Cluiche::AppFlow
