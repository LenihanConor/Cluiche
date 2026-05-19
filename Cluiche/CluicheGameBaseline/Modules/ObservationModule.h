#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Session/SessionManager.h>

namespace Dia { namespace Observation { namespace Log { class ISink; } } }

namespace Cluiche { namespace AppFlow {

class ObservationModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit ObservationModule(const Dia::Core::StringCRC& instanceId);
    ~ObservationModule();

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    static constexpr unsigned int kMaxSinks = 8;
    Dia::Observation::Log::ISink* mOwnedSinks[kMaxSinks];
    unsigned int mOwnedSinkCount = 0;
    Dia::Observation::SessionManager mSessionManager;
};

} } // namespace Cluiche::AppFlow
