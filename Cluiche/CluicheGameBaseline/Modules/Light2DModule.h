////////////////////////////////////////////////////////////////////////////////
// Filename: Light2DModule.h
// Description: Owns the LightRegistry2D for the SimPU.
//              Exposes the registry to sibling modules (e.g. Scene2DModule
//              loads scene lights into it; renderers read from it).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>

namespace Cluiche { namespace AppFlow {

class Light2DModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Owns the LightRegistry2D for the SimPU";

    explicit Light2DModule(const Dia::Core::StringCRC& instanceId);

    Dia::Lighting2D::LightRegistry2D&       GetRegistry()       { return mRegistry; }
    const Dia::Lighting2D::LightRegistry2D& GetRegistry() const { return mRegistry; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
    Dia::Lighting2D::LightRegistry2D mRegistry;
};

} } // namespace Cluiche::AppFlow
