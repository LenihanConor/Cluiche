#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaInput/InputSourceManager.h>
#include <DiaInput/EventData.h>
#include <DiaInput/ConsoleGamepadManager.h>
#include <DiaSFML/RenderWindowFactory.h>
#include "Types/InputEvent.h"

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

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::EventStreamWriter<InputEvent> mInputWriter{this, "InputToSim"};
    static Dia::Graphics::ICanvas*    sCanvas;
    static Dia::SFML::TextureHandler* sTextureHandler;

    Dia::Input::InputSourceManager mInputSourceManager;
    Dia::Input::ConsoleGamepadManager mGamepadManager;
    Dia::SFML::RenderWindowFactory mWindowFactory;
    Dia::Window::IWindow*   mWindow = nullptr;
    Dia::Graphics::ICanvas* mCanvas = nullptr;
    Dia::Input::EventData   mFrameEvents;
};

} } // namespace Cluiche::AppFlow
