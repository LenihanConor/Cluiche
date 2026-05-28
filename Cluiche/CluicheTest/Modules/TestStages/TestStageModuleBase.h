#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/AutomationModule.h"

namespace Dia { namespace Automation { class AutomationService; } }

namespace CluicheTest {

class TestStageModuleBase : public Dia::ApplicationFlow::Module
{
public:
    explicit TestStageModuleBase(const Dia::Core::StringCRC& instanceId);
    ~TestStageModuleBase() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() final;
    void DoUpdate(float deltaTime) final;
    Dia::ApplicationFlow::StopResult DoStop() final;

    virtual Dia::Core::StringCRC GetStageName() const = 0;
    virtual unsigned int GetBudgetFrames() const = 0;
    virtual const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const = 0;

    virtual bool AreDependenciesReady() { return true; }
    virtual void OnStart(Dia::Automation::AutomationService* service) = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnStop() {}
    virtual void OnTimeout() {}
    virtual bool PersistsAcrossEntries() const { return false; }

    void ReportPassed();
    void ReportFailed();
    unsigned int GetFrameCount() const { return mFrameCount; }
    unsigned int GetEntryCount() const { return mEntryCount; }
    bool IsResolved() const { return mResolved; }
    Dia::Automation::AutomationService* GetAutomationService();

private:
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule> mAutomation{this};
    unsigned int mFrameCount = 0;
    unsigned int mEntryCount = 0;
    bool mResolved = false;
};

} // namespace CluicheTest
