#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Threading/JobSystem.h>

namespace Cluiche { namespace AppFlow {

class JobSystemModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit JobSystemModule(const Dia::Core::StringCRC& instanceId);

    static JobSystemModule*      GetStatic();
    Dia::Core::JobSystem&        GetJobSystem();
    const Dia::Core::JobSystem&  GetJobSystem() const;

protected:
    Dia::ApplicationFlow::StartResult DoStart()         override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()          override;

private:
    static JobSystemModule* sInstance;
    Dia::Core::JobSystem    mJobSystem;
};

} } // namespace Cluiche::AppFlow
