#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Core/RandomStreamManager.h>
#include <DiaMaths/Core/RandomStream.h>

namespace CoW {

// Manages the 8 independent RNG streams for CoW F-2 Core Runtime determinism.
// Streams: WLD (world), A1, B4, B6, C1, C2, B5, C4
// Each stream is seeded independently to ensure deterministic simulation across systems.
class RandomStreamModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "8 independent RNG streams for deterministic F-2 simulation";

    explicit RandomStreamModule(const Dia::Core::StringCRC& instanceId);

    // Stream accessors (returns nullptr if called before DoStart)
    Dia::Maths::RandomStream* GetWorldStream();      // WLD - world generation
    Dia::Maths::RandomStream* GetA1Stream();         // A1
    Dia::Maths::RandomStream* GetB4Stream();         // B4
    Dia::Maths::RandomStream* GetB6Stream();         // B6
    Dia::Maths::RandomStream* GetC1Stream();         // C1
    Dia::Maths::RandomStream* GetC2Stream();         // C2
    Dia::Maths::RandomStream* GetB5Stream();         // B5
    Dia::Maths::RandomStream* GetC4Stream();         // C4

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::Maths::RandomStreamManager mManager;
};

} // namespace CoW
