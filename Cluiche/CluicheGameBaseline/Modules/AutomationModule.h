////////////////////////////////////////////////////////////////////////////////
// Filename: AutomationModule.h
// CluicheGameBaseline — wires AutomationService into the app lifecycle.
//
// Instantiates AutomationService in DoStart, registers commands, enables
// navigation hold. AutomationService lives as long as this module lives.
// This module uses stages: ["all"] so it runs for the full session.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>

namespace Dia { namespace Automation { class AutomationService; } }

namespace Cluiche { namespace AppFlow {

class AutomationModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit AutomationModule(const Dia::Core::StringCRC& instanceId);
    ~AutomationModule();

    Dia::Automation::AutomationService* GetService() { return mService.Get(); }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::Core::UniquePtr<Dia::Automation::AutomationService> mService;
};

} } // namespace Cluiche::AppFlow
