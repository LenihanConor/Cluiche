#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Session/SessionManager.h>

namespace Dia { namespace Observation { namespace Log { class ISink; } } }

namespace Dia { namespace ApplicationFlow {

// Reusable observation module: wires logging sinks, starts DiaObservation session,
// and drives SessionManager::Tick each frame.
//
// Applications instantiate this directly or subclass it to override
// ApplyConfigOverrides() (e.g. to layer CLI flags on top of the loaded config).
class ObservationModule : public Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr PUAffinity kAllowedPUs = PUAffinity::kAny;

    // appName        — written into SessionConfig::appName (e.g. "CluicheTest")
    // configAsset    — repo-relative path from the exe dir to the .diaobservation
    //                  asset, e.g. "../../../../Assets/CluicheTest/cluichetest.diaobservation"
    ObservationModule(const Dia::Core::StringCRC& instanceId,
                      const char* appName,
                      const char* configAssetRelPath);

protected:
    StartResult DoStart() override;
    void        DoUpdate(float dt) override;
    StopResult  DoStop() override;

    // Override to layer additional config on top of the loaded ObservationConfig
    // before the session starts (e.g. apply CLI flags).
    virtual void ApplyConfigOverrides(Dia::Observation::ObservationConfig& /*config*/) {}

private:
    static constexpr unsigned int kMaxSinks = 8;
    Dia::Observation::Log::ISink* mOwnedSinks[kMaxSinks];
    unsigned int mOwnedSinkCount = 0;
    Dia::Observation::SessionManager mSessionManager;

    const char* mAppName;
    const char* mConfigAssetRelPath;
};

} } // namespace Dia::ApplicationFlow
