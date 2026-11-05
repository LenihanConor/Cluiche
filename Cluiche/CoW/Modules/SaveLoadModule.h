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
#include <DiaCore/Json/external/json/json.h>
#include "Modules/SnapshotStoreAdapter.h"
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

// One participant's id and the JSON it would write (see SaveLoadModule::CaptureState).
struct CapturedState
{
    Dia::Core::StringCRC id;
    Json::Value          state;
};

// Fixed-capacity result of CaptureState, in registration order.
class CapturedStateList
{
public:
    static constexpr uint32_t kCapacity = Dia::SaveGame::SaveRegistry::kMaxParticipants;
    uint32_t             Size() const { return mCount; }
    const CapturedState& operator[](uint32_t i) const { return mItems[i]; }
    void                 Clear() { mCount = 0; }
    CapturedState&       Append() { return mItems[mCount++]; }   // callers stay within kCapacity
private:
    CapturedState mItems[kCapacity];
    uint32_t      mCount = 0;
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
    // Returns false (and logs) for a null participant, a duplicate id or a full registry; the
    // registry is left unchanged. Unregistering an unknown id is a logged no-op.
    bool RegisterParticipant(Dia::Core::StringCRC id, Dia::SaveGame::ISaveable* participant);
    void UnregisterParticipant(Dia::Core::StringCRC id);

    // Record a request; it runs at the next DoUpdate, never inline. Last request wins per kind.
    // Returns false (and records nothing) when the slot id fails SlotManager::IsValidSlotId.
    bool RequestSave(Dia::Core::StringCRC slotId);
    bool RequestLoad(Dia::Core::StringCRC slotId);
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

    // ---- Snapshots (docs/specs/.../e1-saveload/snapshot-store-adapter.md) ----
    // The store DiaCellHistory's SnapshotManager takes: named game states in <base>/snapshots/<name>.sav,
    // outside the slots (never listed by cow.save.list, never counted against maxSlots).
    Dia::CellHistory::ISnapshotStore& GetSnapshotStore() { return mSnapshots; }
    SnapshotStoreAdapter&             GetSnapshotAdapter() { return mSnapshots; }
    const char*                       GetSnapshotFolder() const { return mSnapshotFolder; }   // no trailing slash
    uint32_t GetSnapshotTakeCount() const    { return mSnapshotTakeCount; }
    uint32_t GetSnapshotRestoreCount() const { return mSnapshotRestoreCount; }
    uint32_t GetSnapshotFailureCount() const { return mSnapshotFailureCount; }

    // Each registered participant's id and the JSON it would write, in registration order. Writes no
    // slot, does no disk I/O, and changes neither game state, counters nor the pending flags.
    // Call at the sim-frame boundary (like a save). Participants beyond kCapacity cannot exist.
    void CaptureState(CapturedStateList& out) const;

protected:
    void OnConfigure(const char* configJson) override;
    // Runs before any module's DoStart: hands our registry to a sibling DiaSimTimeModule (if the
    // PU has one, instance id == DiaSimTimeModule::kTypeId) so its clock/scheduler state persists.
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    friend class SnapshotStoreAdapter;
    static constexpr uint32_t kSnapshotPathCapacity = 512;

    void BuildSnapshotPath(const char* name, char* out, uint32_t capacity) const;
    void UpdateSnapshotPaths();
    // Run by the adapter at the frame boundary. They point the config at the snapshot folder for the
    // one call (folder, "{id}.sav", unlimited slots) and restore it afterwards.
    bool SnapshotSave(const char* name, char* message, uint32_t messageCapacity);
    bool SnapshotLoad(const char* name, char* message, uint32_t messageCapacity);
    void NoteSnapshotFailure(bool isTake, const char* name, const char* reason);

    void ExecuteSave(Dia::Core::StringCRC slotId);
    void ExecuteLoad(Dia::Core::StringCRC slotId);
    // Reads the slot file and checks it would load. Returns Ok or the failing code.
    Dia::SaveGame::LoadResult PreValidateLoad(Dia::Core::StringCRC slotId);
    bool FindParticipant(Dia::Core::StringCRC id, uint32_t& outIndex) const;
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

    char                           mSnapshotFolder[kBaseDirCapacity + 16] = {};   // "<base>snapshots"
    char                           mSnapshotPrefix[kBaseDirCapacity + 16] = {};   // "<base>snapshots/"
    SnapshotStoreAdapter           mSnapshots{*this};
    uint32_t                       mSnapshotTakeCount = 0;
    uint32_t                       mSnapshotRestoreCount = 0;
    uint32_t                       mSnapshotFailureCount = 0;
    bool                           mLastSnapshotFailed = false;
    Dia::Observation::Metric::Counter* mSnapshotTakeCounter = nullptr;
    Dia::Observation::Metric::Counter* mSnapshotRestoreCounter = nullptr;
    Dia::Observation::Metric::Counter* mSnapshotFailCounter = nullptr;

    SaveLoadHealthReporter         mHealth;
    bool                           mHealthRegistered = false;

    Dia::Observation::Metric::Counter* mSaveCounter = nullptr;
    Dia::Observation::Metric::Counter* mSaveFailCounter = nullptr;
    Dia::Observation::Metric::Counter* mLoadCounter = nullptr;
    Dia::Observation::Metric::Counter* mLoadFailCounter = nullptr;
};

} // namespace CoW
