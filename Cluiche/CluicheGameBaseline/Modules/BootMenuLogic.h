#pragma once
#include <Modules/StageStatusModule.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

// Pure (no ImGui, no module machinery) logic behind BootMenuModule: stage list, visited
// bookkeeping, status lookup and severity counts. The provider may be null.
class BootMenuLogic {
public:
    static constexpr unsigned int kMaxStages = 64;
    static constexpr const char* kDefaultTitle = "Boot Menu";

    struct Counts {
        unsigned int passed = 0;
        unsigned int failed = 0;
        unsigned int pending = 0;
    };

    struct Badge {
        const char* label = nullptr;
        StageStatusModule::Severity severity = StageStatusModule::Severity::kNone;
    };

    static constexpr unsigned int kMaxPresetsPerStage = 16;   // matches the .diastage limit
    static constexpr unsigned int kMaxPresets = 128;          // pool shared by all stages
    static constexpr unsigned int kMaxDescription = 121;      // 120 chars + NUL

    struct PresetInfo {
        Dia::Core::StringCRC name;
        char description[kMaxDescription] = {};
    };

    // One menu row: a stage row (isPreset false, preset empty = launch with no preset) or a preset
    // sub-row under its stage (description is the tooltip).  Click: launch row.stage with row.preset.
    struct Row {
        Dia::Core::StringCRC stage;
        Dia::Core::StringCRC preset;
        bool isPreset = false;
        char description[kMaxDescription] = {};
    };

    // Replaces the stage list and clears visited markers and presets.
    void SetStages(const Dia::Core::StringCRC* stages, unsigned int count);
    // Replaces the presets shown under `stage` (a stage not in the list is ignored).  Over the
    // per-stage or pool cap the extras are dropped.
    void SetStagePresets(const Dia::Core::StringCRC& stage, const PresetInfo* presets, unsigned int count);
    // Stage rows in list order, each followed by its preset sub-rows.
    unsigned int RowCount() const;
    bool GetRow(unsigned int index, Row& outRow) const;

    // Provider is optional; null means no badges and no counts.
    void SetProvider(const StageStatusModule* provider);

    bool HasStatus() const;
    // Fills badge and returns true only if a provider exists and has an entry for the stage.
    bool GetBadge(const Dia::Core::StringCRC& stage, Badge& outBadge) const;
    // Passed = kOk, Failed = kFail, Pending = everything else (incl. no entry). Zeros if no provider.
    Counts CountBySeverity() const;

    // Marks every listed stage visited whose provider entry has visited == true.
    void RestoreVisited();
    // Marks a listed stage visited (called when the nav request is sent).
    void MarkVisited(const Dia::Core::StringCRC& stage);
    bool IsVisited(const Dia::Core::StringCRC& stage) const;

    // Null or empty config value -> default title / empty project line.
    static const char* ResolveTitle(const char* configValue);
    static const char* ResolveProject(const char* configValue);

private:
    Dia::Core::StringCRC mStages[kMaxStages];
    bool mVisited[kMaxStages] = {};
    unsigned int mCount = 0;
    struct PresetEntry { Dia::Core::StringCRC stage; PresetInfo info; };
    PresetEntry mPresets[kMaxPresets];
    unsigned int mPresetCount = 0;
    const StageStatusModule* mProvider = nullptr;
};

} } // namespace Cluiche::AppFlow
