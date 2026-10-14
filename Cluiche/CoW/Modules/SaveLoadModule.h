#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaSaveGame/SaveResult.h>
#include <DiaSaveGame/SlotInfo.h>
#include <DiaSaveGame/SlotManager.h>
#include <cstdint>

namespace CoW {

// Failing: the save directory could not be created, or saves have failed repeatedly in a row
// (kFailingAfterConsecutiveSaveFailures). Degraded: the most recent save or load failed.
// OK again as soon as the next save / load of that kind succeeds.
class SaveLoadHealthReporter : public Dia::Observation::Health::HealthReporterBase
{
public:
    SaveLoadHealthReporter() { SetOK(); }
    Dia::Core::StringCRC GetReporterName() const override { return Dia::Core::StringCRC("SaveLoadModule"); }
};

// Owns the DiaSaveGame SaveRegistry/SaveManager for CoW and executes save/load requests at the
// sim-frame boundary. Requests (direct or via the cow.save / cow.load commands) are only recorded;
// DoUpdate runs them. Other systems register ISaveable participants; this shell ships with none.
// A load is pre-validated (slot present, JSON parses, manifest readable and compatible) before
// SaveManager::Load runs, so a bad file leaves the game untouched.
// Spec: docs/specs (E1 Save/Load)
class SaveLoadModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Save/load orchestration over DiaSaveGame";

    explicit SaveLoadModule(const Dia::Core::StringCRC& instanceId);
    ~SaveLoadModule() override;

    // The E1 defaults: out/saves/cow/, slot_{id}.sav, unlimited slots, JSON.
    static Dia::SaveGame::SaveConfig DefaultConfig();
    // The live config (differs from the default only if the manifest overrides base_directory).
    const Dia::SaveGame::SaveConfig& GetConfig() const { return mConfig; }

    // Registration order is serialization order. Capped at SaveRegistry::kMaxParticipants.
    void RegisterParticipant(Dia::Core::StringCRC id, Dia::SaveGame::ISaveable* participant);
    void UnregisterParticipant(Dia::Core::StringCRC id);

    // Record a request; it runs at the next DoUpdate, never inline. Last request wins per kind.
    void RequestSave(Dia::Core::StringCRC slotId);
    void RequestLoad(Dia::Core::StringCRC slotId);
    bool HasPendingSave() const { return mHasPendingSave; }
    bool HasPendingLoad() const { return mHasPendingLoad; }

    void EnumerateSlots(Dia::Core::Containers::DynamicArrayC<Dia::SaveGame::SlotInfo, 32>& out) const;

    // So the app can inject it into DiaSimTimeModule::SetSaveRegistry.
    Dia::SaveGame::SaveRegistry& GetRegistry() { return mRegistry; }

    // Observable results.
    Dia::SaveGame::SaveResult GetLastSaveResult() const { return mLastSaveResult; }
    Dia::SaveGame::LoadResult GetLastLoadResult() const { return mLastLoadResult; }
    uint32_t GetSaveCount() const { return mSaveCount; }   // completed (successful) saves
    uint32_t GetLoadCount() const { return mLoadCount; }   // completed (successful) loads

    static constexpr uint32_t kFailingAfterConsecutiveSaveFailures = 3;
    const SaveLoadHealthReporter& GetHealthReporter() const { return mHealth; }

protected:
    void OnConfigure(const char* configJson) override;
    // Runs before any module's DoStart: hands our registry to a sibling DiaSimTimeModule (if the
    // PU has one, instance id == DiaSimTimeModule::kTypeId) so its clock/scheduler state persists.
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void ExecuteSave(Dia::Core::StringCRC slotId);
    void ExecuteLoad(Dia::Core::StringCRC slotId);
    // Reads the slot file and checks it would load. Returns Ok or the failing code.
    Dia::SaveGame::LoadResult PreValidateLoad(Dia::Core::StringCRC slotId);
    void UpdateHealth();
    void RegisterCommands();
    void UnregisterCommands();

    static constexpr unsigned int kBaseDirCapacity = 256;

    char                           mBaseDirectory[kBaseDirCapacity];
    Dia::SaveGame::SaveConfig      mConfig;
    Dia::SaveGame::SaveRegistry    mRegistry;
    Dia::SaveGame::SaveManager     mManager;
    Dia::SaveGame::SlotManager     mSlots;   // path/exists queries for pre-validation
    bool                           mStarted = false;
    bool                           mCommandsRegistered = false;

    bool                           mHasPendingSave = false;
    bool                           mHasPendingLoad = false;
    Dia::Core::StringCRC           mPendingSaveSlot;
    Dia::Core::StringCRC           mPendingLoadSlot;

    Dia::SaveGame::SaveResult      mLastSaveResult = Dia::SaveGame::SaveResult::Success();
    Dia::SaveGame::LoadResult      mLastLoadResult = Dia::SaveGame::LoadResult::Success();
    uint32_t                       mSaveCount = 0;
    uint32_t                       mLoadCount = 0;
    uint32_t                       mConsecutiveSaveFailures = 0;
    bool                           mSaveDirFailed = false;

    SaveLoadHealthReporter         mHealth;
    bool                           mHealthRegistered = false;

    Dia::Observation::Metric::Counter* mSaveCounter = nullptr;
    Dia::Observation::Metric::Counter* mSaveFailCounter = nullptr;
    Dia::Observation::Metric::Counter* mLoadCounter = nullptr;
    Dia::Observation::Metric::Counter* mLoadFailCounter = nullptr;
};

} // namespace CoW
