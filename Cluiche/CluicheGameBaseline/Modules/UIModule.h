#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaUI/UIDataBuffer.h>
#include "Types/SimToMainEvent.h"
#include "Modules/KernelModule.h"

namespace Dia { namespace UI { class IUISystem; class Page; } }
namespace Dia { namespace UI { namespace Ultralight { class UISystem; } } }

namespace Cluiche { namespace AppFlow {

// UIModule owns the Ultralight UI system for the whole process lifetime
// (stage = "all"). It publishes each frame's UI pixel buffer on the UIToSim
// stream so the sim/render path can composite the HUD.
//
// UIModule does NOT own a page. Per-stage "UI page" modules (BootUIPageModule,
// DummyUIPageModule) own their C++ Page shell and call LoadPage / UnloadPage
// on this module through a ModuleRef. When a stage transition fires, the old
// stage's page module stops (UnloadPage) and the new one starts (LoadPage) —
// UIModule itself stays running so there's no UISystem teardown between
// stages.
class UIModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Ultralight UI page lifecycle and JS bridge";
    explicit UIModule(const Dia::Core::StringCRC& instanceId);
    ~UIModule();

    // Accessors for page-owner modules and for AssetServiceModule (type handler).
    Dia::UI::IUISystem* GetUISystem() { return reinterpret_cast<Dia::UI::IUISystem*>(mUISystem); }
    bool HasStarted() const { return mHasStarted; }

    // Called by page-owner modules on their DoStart / DoStop.
    void LoadPage(Dia::UI::Page& page);
    void UnloadPage();

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::EventStreamReader<SimToMainEvent> mUICommands{this, "SimToMain"};
    Dia::ApplicationFlow::StreamWriter<Dia::UI::UIDataBuffer> mUIBufferOutput{this, "UIToSim"};
    Dia::ApplicationFlow::ModuleRef<KernelModule> mKernel{this};

    Dia::UI::Ultralight::UISystem* mUISystem = nullptr;
    bool mHasStarted = false;
};

} } // namespace Cluiche::AppFlow
