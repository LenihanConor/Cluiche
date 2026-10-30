#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include "Modules/SimQueryHostModule.h"

namespace CluicheTest {

// Test fixture for Sim-Hosted Queries: registers the sim-hosted query "sim.echo"
// that returns {"echo": <args>, "thread": "sim"} from the SimPU thread.
class SimEchoTestModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Registers the sim.echo sim-hosted debug query (test fixture)";

    explicit SimEchoTestModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::SimQueryHostModule> mHost{this};
    bool mRegistered = false;
};

} // namespace CluicheTest

#endif // DIA_DEBUG
