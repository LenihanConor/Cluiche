#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/EventData.h>
#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaSFML/RenderWindowFactory.h>
#include "Types/InputEvent.h"

#include <atomic>

namespace Dia { namespace Graphics { class ICanvas; } }
namespace Dia { namespace Window { class IWindow; } }
namespace Dia { namespace SFML { class TextureHandler; } }

namespace Cluiche { namespace AppFlow {

class KernelModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit KernelModule(const Dia::Core::StringCRC& instanceId);

    Dia::Graphics::ICanvas* GetCanvas() { return mCanvas; }
    Dia::Window::IWindow*   GetWindow() { return mWindow; }

    // Input events gathered this frame by DoUpdate. Exposed so same-PU
    // consumers (UIModule) can inject them into the UI system without
    // going through the InputToSim stream (which is cross-PU).
    const Dia::Input::EventData& GetFrameInputEvents() const { return mFrameEvents; }

    // Static accessors: set in DoStart, cleared in DoStop.
    // Allows cross-PU consumers (RenderModule on RenderPU, DummyLevelModule
    // on SimPU) to retrieve the canvas / texture handler without a direct
    // same-PU module reference.
    static Dia::Graphics::ICanvas*     GetStaticCanvas()         { return sCanvas; }
    static Dia::SFML::TextureHandler*  GetStaticTextureHandler() { return sTextureHandler; }

    // Cross-PU shutdown fence. RenderModule (RenderPU) sets this true after it
    // has released the GL context in DoStop; KernelModule (MainPU) blocks in
    // DoStop until it observes true, then destroys the window. Without this
    // fence the two DoStops race: KernelModule can tear down the SFML window
    // (and call setActive(true) on MainPU) while RenderPU is still issuing GL
    // calls, causing a driver hang inside glDeleteFramebuffers / wglMakeCurrent.
    // RenderModule::DoStart resets it false on each (re-)entry.
    static void SetRenderContextReleased(bool released) { sRenderContextReleased.store(released, std::memory_order_release); }
    static bool IsRenderContextReleased()               { return sRenderContextReleased.load(std::memory_order_acquire); }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::EventStreamWriter<InputEvent> mInputWriter{this, "InputToSim"};
    static Dia::Graphics::ICanvas*    sCanvas;
    static Dia::SFML::TextureHandler* sTextureHandler;
    static std::atomic<bool>          sRenderContextReleased;

    Dia::Input::InputSourceManager mInputSourceManager;
    Dia::Input::ConsoleGamepadManager mGamepadManager;
    Dia::SFML::RenderWindowFactory mWindowFactory;
    Dia::Window::IWindow*   mWindow = nullptr;
    Dia::Graphics::ICanvas* mCanvas = nullptr;
    Dia::Input::EventData   mFrameEvents;
};

} } // namespace Cluiche::AppFlow
