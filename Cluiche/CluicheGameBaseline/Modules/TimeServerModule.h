#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Time/TimeServer.h>

namespace Cluiche { namespace AppFlow {

// TimeServerModule (Sim PU): wraps Dia::Core::TimeServer ticked at the Sim
// PU frequency. Same-PU consumers (InputStreamModule, DummyLevelModule) access
// the server via GetTimeServer() to query absolute/relative times consistently.
class TimeServerModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Frame timing and delta-time provider";
    explicit TimeServerModule(const Dia::Core::StringCRC& instanceId);

    const Dia::Core::TimeServer& GetTimeServer() const { return mTimeServer; }
    Dia::Core::TimeServer&       GetTimeServer()       { return mTimeServer; }

    float        GetDeltaTime()  const;
    float        GetTotalTime()  const;
    unsigned int GetFrameCount() const;

protected:
    Dia::ApplicationFlow::StartResult DoStart()               override;
    void                              DoUpdate(float dt)       override;
    Dia::ApplicationFlow::StopResult  DoStop()                override;

private:
    Dia::Core::TimeServer mTimeServer;
};

} } // namespace Cluiche::AppFlow
