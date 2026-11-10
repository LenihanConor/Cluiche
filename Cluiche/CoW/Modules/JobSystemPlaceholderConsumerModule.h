#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobSystem.h>

namespace CoW {

// PLACEHOLDER. Remove when A4 Weather lands its real JobSystemService consumer.
//
// JobSystemModule (MainPU) provides JobSystemService, and the manifest validator
// fails at launch on a ServiceStream provider with no consumer
// (SERVICE_STREAM_ORPHAN_PROVIDER, decision in service-channel.md). CoW has no
// consumer until A4's parallel weekly update (dia-needs #12) is built, so this
// module only connects the reader so the stream is not an orphan. It does no work.
//
// To remove: delete this module, its entry in cow_main.diaapp, and the vcxproj
// entries, once a real module declares `role: consumes` for JobSystemService.
class JobSystemPlaceholderConsumerModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Placeholder consumer so JobSystemService is not an orphan provider until A4 lands";

    explicit JobSystemPlaceholderConsumerModule(const Dia::Core::StringCRC& instanceId);

protected:
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Threading::JobSystem> mJobSystemReader{this, "JobSystemService"};
};

} // namespace CoW
