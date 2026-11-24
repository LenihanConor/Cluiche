#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/EventStreamWriter.h>
#include "Modules/TestResultChangeTracker.h"
#include "Types/StageResult.h"

namespace CluicheTest {

// Polls TestResultsRegistry on the SimPU (the thread that writes it) and sends a StageResultEvent on the
// "StageResult" EventStream (SimPU -> MainPU) whenever a stage's state changes.
class TestResultReporterModule : public Dia::ApplicationFlow::SimModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr const char* kDescription = "Reports TestResultsRegistry state changes to the StageCatalogue";
    explicit TestResultReporterModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::EventStreamWriter<Cluiche::AppFlow::StageResultEvent> mResultOut{this, "StageResult"};
    TestResultChangeTracker mTracker;
};

} // namespace CluicheTest
