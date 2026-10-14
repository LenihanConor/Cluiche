#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Core/RandomStreamManager.h>
#include <DiaMaths/Core/RandomStream.h>
#include <DiaSaveGame/ISaveable.h>
#include "Modules/SaveLoadModule.h"

namespace CoW {

class RandomStreamModule;

// E1 save participant: each stream persists as seed + draw count and is replayed on restore.
class RandomStreamSaveParticipant : public Dia::SaveGame::ISaveable
{
public:
    explicit RandomStreamSaveParticipant(RandomStreamModule& module) : mModule(module) {}
    void Serialize(Dia::SaveGame::SaveContext& ctx) const override;
    void Deserialize(Dia::SaveGame::LoadContext& ctx) override;
    uint32_t GetVersion() const override { return 1; }

private:
    RandomStreamModule& mModule;
};

// Manages the 8 independent RNG streams for CoW F-2 Core Runtime determinism.
// Streams: WLD (world), A1, B4, B6, C1, C2, B5, C4
// Each stream is seeded independently to ensure deterministic simulation across systems.
class RandomStreamModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "8 independent RNG streams for deterministic F-2 simulation";

    // Single source of truth for the stream set (shared by creation and the save participant).
    static constexpr int kStreamCount = 8;
    static constexpr const char* kStreamNames[kStreamCount] = { "WLD", "A1", "B4", "B6", "C1", "C2", "B5", "C4" };

    explicit RandomStreamModule(const Dia::Core::StringCRC& instanceId);

    // Returns nullptr if the stream does not exist (e.g. before DoStart).
    Dia::Maths::RandomStream* GetStreamByName(const Dia::Core::StringCRC& name) { return mManager.GetStream(name); }
    // The ISaveable registered with SaveLoadModule when one exists (exposed for tests).
    Dia::SaveGame::ISaveable& GetSaveParticipant() { return mSaveParticipant; }

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
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void TryRegisterSaveParticipant();
    void UnregisterSaveParticipant();

    Dia::Maths::RandomStreamManager mManager;
    Dia::ApplicationFlow::ModuleRef<SaveLoadModule> mSaveLoad{this};
    RandomStreamSaveParticipant mSaveParticipant{*this};
    bool mSaveParticipantRegistered = false;
};

} // namespace CoW
