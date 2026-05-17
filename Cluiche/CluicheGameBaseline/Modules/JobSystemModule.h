#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

class JobSystemModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit JobSystemModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart()         override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()          override;
};

} } // namespace Cluiche::AppFlow
