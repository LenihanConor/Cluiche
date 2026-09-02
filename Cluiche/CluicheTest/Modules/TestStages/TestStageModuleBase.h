#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/LifecycleEvent.h>
#include <DiaStreams/StreamReader.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Frame/RenderFence.h>
#include <atomic>
#include <cstdint>

namespace Dia { namespace Automation { class AutomationService; } }
namespace Dia { namespace ApplicationFlow { template<typename T> class ServiceStreamReader; } }

namespace CluicheTest {

class TestStageModuleBase : public Dia::ApplicationFlow::SimModule
{
public:
    explicit TestStageModuleBase(const Dia::Core::StringCRC& instanceId);
    ~TestStageModuleBase() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() final;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) final;
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

protected:
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void FireCapture();
    void WriteMetrics(bool passed);

    // Heap-allocated to avoid pulling AutomationService.h (and Application.h) into
    // every TU that includes this header. Constructed in TestStageModuleBase().
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>* mAutomationServiceStream = nullptr;
    Dia::ApplicationFlow::StreamReader<Dia::Graphics::RenderFence> mRenderFence{this, "RenderToSim"};

    // Set by a $lifecycle tap (fires on MainPU), read by DoUpdate on SimPU.
    // atomic<bool> is the correct primitive for this single flag handoff.
    std::atomic<bool> mAbortRequested{false};
    Dia::ApplicationFlow::TapHandle mLifecycleTap;
    Dia::ApplicationFlow::EventStreamStore<Dia::ApplicationFlow::LifecycleEvent>* mLifecycleStore = nullptr;

    unsigned int mFrameCount = 0;
    unsigned int mEntryCount = 0;
    unsigned int mStartWaitFrames = 0;
    bool mResolved = false;
    bool mStarted = false;
    bool mNavigationReleased = false;

    // Deferred capture: set by ReportPassed/Failed, fired once render confirms the frame.
    bool mAwaitingCapture = false;
    bool mCaptureWasPassed = false;
    uint64_t mCaptureFrameTarget = 0;
};

} // namespace CluicheTest
