#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "UI/DummyUIPage.h"
#include "Modules/UIModule.h"

namespace Cluiche { namespace AppFlow {

// DummyUIPageModule: stage-specific (DummyStage) Main-PU module that owns
// the DummyUIPage C++ shell and drives UIModule LoadPage/UnloadPage for
// the DummyStage stage. Application_ExitLevel (from the HTML) triggers
// a return TransitionTo("Boot").
class DummyUIPageModule
    : public Dia::ApplicationFlow::Module
    , public Cluiche::DummyStage::DummyUIPageExternalInterface
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit DummyUIPageModule(const Dia::Core::StringCRC& instanceId);

    // DummyUIPageExternalInterface — called from Application_ExitLevel JS.
    void RequestExitLevel() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::ApplicationFlow::ModuleRef<UIModule> mUI{this};
    Cluiche::DummyStage::DummyUIPage mPage{this};
    bool mLoaded = false;
};

} } // namespace Cluiche::AppFlow
