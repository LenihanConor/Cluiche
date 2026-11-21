#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaThreading/JobSystem.h>
#include <atomic>

namespace Cluiche { namespace Test {

// JobSystemServiceTestStageModule
// ---------------------------------------------------------------------------
// Integration test: proves SimPU can consume JobSystemService from MainPU
// and safely call TrySubmit, with graceful fallback when unavailable.
// ---------------------------------------------------------------------------
class JobSystemServiceTestStageModule : public Dia::ApplicationFlow::SimModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Integration test for cross-PU JobSystemService access";

    explicit JobSystemServiceTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Threading::JobSystem> mJobSystemReader{this, "JobSystemService"};

    std::atomic<int> mParallelJobCounter{0};
    std::atomic<int> mSerialFallbackCounter{0};
    int mFrameCount = 0;
    bool mTestCompleted = false;
};

} } // namespace Cluiche::Test
