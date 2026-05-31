#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobSystem.h>

namespace Cluiche { namespace AppFlow {

class JobSystemModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Thread pool for parallel job dispatch";
    explicit JobSystemModule(const Dia::Core::StringCRC& instanceId);

    Dia::Threading::JobSystem&       GetJobSystem();
    const Dia::Threading::JobSystem& GetJobSystem() const;

protected:
    Dia::ApplicationFlow::StartResult DoStart()         override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()          override;

private:
    Dia::Threading::JobSystem mJobSystem;
};

} } // namespace Cluiche::AppFlow
