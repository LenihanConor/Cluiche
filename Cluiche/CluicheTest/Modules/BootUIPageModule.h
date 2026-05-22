#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "Pages/LaunchUIPage.h"
#include "Modules/UIModule.h"

namespace Cluiche { namespace AppFlow {

// BootUIPageModule: stage-specific (Boot) Main-PU module that owns the
// LaunchUIPage C++ shell (bootscreen.html) and drives UIModule::LoadPage/
// UnloadPage for the Boot stage. When the bootscreen JS calls
// Application_LaunchLevel, this module queues a stage transition to
// "DummyStage" via Application::TransitionTo.
class BootUIPageModule
    : public Dia::ApplicationFlow::Module
    , public Cluiche::LaunchUIPageExternalInterface
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit BootUIPageModule(const Dia::Core::StringCRC& instanceId);

    // LaunchUIPageExternalInterface
    void RequestLaunchLevel(const Dia::Core::Containers::String64& levelName) override;
    int GetNavigableStageCount() override;
    Dia::Core::Containers::String64 GetNavigableStageName(int index) override;
    Dia::Core::Containers::String64 GetStageStatus(int index) override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void CacheNavigableStages();

    Dia::ApplicationFlow::ModuleRef<UIModule> mUI{this};
    Cluiche::LaunchUIPage mPage{this};
    bool mLoaded = false;

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> mNavigableStages;
};

} } // namespace Cluiche::AppFlow
