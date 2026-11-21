#pragma once
#include <Types/StageCatalogueSnapshot.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Cluiche { namespace AppFlow {

// Pure (no ImGui, no module machinery) view-model behind BootMenuModule, built over a StageCatalogueSnapshot.
// Groups (Showcase, Tools, Tests, Other) -> rows; a stage with N kinds has N rows. Selection, visited and result
// belong to the stage, not the row. Filter, collapse, selection and visited state survive SetSnapshot.
// Pointers returned by accessors (names, labels, descriptions) are valid until the next SetSnapshot/SetFallbackStages.
class BootMenuLogic {
public:
    static constexpr unsigned int kMaxStages = StageCatalogueSnapshot::kMaxEntries;
    static constexpr unsigned int kMaxRows = 3 * kMaxStages;
    static constexpr unsigned int kGroupCount = kStageGroupCount;
    static constexpr unsigned int kMaxChips = 128;
    static constexpr unsigned int kMaxActiveChips = 8;
    static constexpr unsigned int kChipChars = StageCatalogueSnapshotEntry::kTagChars;
    static constexpr unsigned int kMaxSearch = 64;
    static constexpr unsigned int kMaxTitle = 64;
    static constexpr const char* kDefaultTitle = "Boot Menu";
    static constexpr const char* kFallbackHeading = "Stages";

    struct Counts {
        unsigned int passed = 0;
        unsigned int failed = 0;
        unsigned int pending = 0;
    };

    enum class BadgeColour : uint8_t { kGrey, kGreen, kAmber, kRed };

    struct Badge {
        const char* label = nullptr;
        StageResultSeverity severity = StageResultSeverity::kNone;
        BadgeColour colour = BadgeColour::kGrey;
    };

    // Header of a visible group (one with at least one row after filtering).
    struct GroupInfo {
        StageGroup group = StageGroup::kOther;
        char title[kMaxTitle] = {};       // "Showcase (3)"; Tests: "Tests (P passed, F failed, X pending)"
        unsigned int stageCount = 0;      // rows in the group after filtering (the number shown in the header)
        bool collapsed = false;
    };

    // One menu row: a stage row (isPreset false, preset empty = launch with no preset) or a preset sub-row under it.
    // Click: launch row.stage with row.preset.
    struct Row {
        Dia::Core::StringCRC stage;
        Dia::Core::StringCRC preset;
        bool isPreset = false;
        const char* name = "";            // stage name (preset name on a sub-row)
        const char* description = "";     // stage description (preset description on a sub-row)
        Dia::Game::StageKindMask kinds = 0;
    };

    // ---- Data ----
    // Rebuilds rows from the snapshot. Keeps selection, visited, collapse and filter state. Leaves fallback mode.
    void SetSnapshot(const StageCatalogueSnapshot& snapshot);
    // Fallback (no catalogue): a flat list under one "Stages" heading; no chips, search, badges or presets.
    void SetFallbackStages(const Dia::Core::StringCRC* stages, unsigned int count);
    bool IsFallback() const { return mFallback; }
    bool HasSnapshot() const { return mHasSnapshot && !mFallback; }
    uint32_t Revision() const { return mRevision; }

    // ---- Groups and rows ----
    unsigned int GroupCount() const { return mVisibleGroupCount; }
    bool GetGroup(unsigned int groupIndex, GroupInfo& outGroup) const;
    // Rows of a visible group, each stage row followed by its preset sub-rows (the UI skips them when collapsed).
    unsigned int RowCount(unsigned int groupIndex) const;
    bool GetRow(unsigned int groupIndex, unsigned int rowIndex, Row& outRow) const;

    // ---- Counts ----
    // Unique stages in the catalogue ("N stages available").
    unsigned int StageCount() const { return mCount; }
    unsigned int VisitedCount() const;
    // Unique test-kind stages: passed = ok/warn, failed = fail, pending = none, missing or visited with no result.
    Counts TestRollup() const;

    // ---- Badges (test stages only) ----
    bool HasBadges() const { return !mFallback && mHasSnapshot; }
    // False (outBadge untouched) for a stage without the test kind, an unknown stage, or in fallback mode.
    bool GetBadge(const Dia::Core::StringCRC& stage, Badge& outBadge) const;
    static unsigned int BadgeColourRgba(BadgeColour colour);   // 0xRRGGBBAA

    // ---- Filters ----
    // Every tag in the catalogue: kinds first (showcase, tool, test), then the rest alphabetical.
    unsigned int ChipCount() const { return mFallback ? 0 : mChipCount; }
    const char* ChipName(unsigned int index) const;
    bool IsChipActive(unsigned int index) const;
    void SetChipActive(unsigned int index, bool active);
    void ToggleChip(unsigned int index) { SetChipActive(index, !IsChipActive(index)); }
    void ClearFilters();
    // Case-insensitive substring over stage name and description. Empty = no search. Truncated to kMaxSearch-1.
    void SetSearch(const char* text);
    const char* Search() const { return mSearch; }
    bool HasActiveFilters() const;

    // ---- Collapse (session-only; Tests starts collapsed) ----
    bool IsCollapsed(StageGroup group) const { return mCollapsed[static_cast<unsigned int>(group)]; }
    void SetCollapsed(StageGroup group, bool collapsed) { mCollapsed[static_cast<unsigned int>(group)] = collapsed; }
    void ToggleCollapsed(StageGroup group) { SetCollapsed(group, !IsCollapsed(group)); }

    // ---- Selection and visited (by stage) ----
    static constexpr unsigned int kMaxVisited = kMaxStages;
    void Select(const Dia::Core::StringCRC& stage) { mSelected = stage; mHasSelection = true; }
    void ClearSelection() { mSelected = Dia::Core::StringCRC(); mHasSelection = false; }
    bool HasSelection() const { return mHasSelection; }
    const Dia::Core::StringCRC& Selected() const { return mSelected; }
    bool IsSelected(const Dia::Core::StringCRC& stage) const { return mHasSelection && mSelected == stage; }
    // Marks a stage visited (called when the nav request is sent). Works for any stage name.
    void MarkVisited(const Dia::Core::StringCRC& stage);
    bool IsVisited(const Dia::Core::StringCRC& stage) const;
    // Select + MarkVisited in one call (launch from any row).
    void Launch(const Dia::Core::StringCRC& stage) { Select(stage); MarkVisited(stage); }

    // Null or empty config value -> default title / empty project line.
    static const char* ResolveTitle(const char* configValue);
    static const char* ResolveProject(const char* configValue);

private:
    static bool EntryInGroup(const StageCatalogueSnapshotEntry& e, StageGroup g);
    bool Matches(const StageCatalogueSnapshotEntry& e) const;
    void Rebuild();
    void BuildChips();
    int FindEntry(const Dia::Core::StringCRC& stage) const;

    StageCatalogueSnapshotEntry mEntries[kMaxStages];
    unsigned int mCount = 0;
    uint32_t mRevision = 0;
    bool mHasSnapshot = false;
    bool mFallback = false;

    // Rows per group (entry indices), filtered. mGroupRows[g][0..mGroupRowCount[g]).
    unsigned int mGroupRows[kGroupCount][kMaxStages];
    unsigned int mGroupRowCount[kGroupCount] = {};
    unsigned int mVisibleGroups[kGroupCount] = {};
    unsigned int mVisibleGroupCount = 0;

    char mChips[kMaxChips][kChipChars] = {};
    unsigned int mChipCount = 0;
    char mActiveChips[kMaxActiveChips][kChipChars] = {};
    unsigned int mActiveChipCount = 0;
    char mSearch[kMaxSearch] = {};

    bool mCollapsed[kGroupCount] = { false, false, true, false };   // Showcase, Tool, Test, Other

    Dia::Core::StringCRC mSelected;
    bool mHasSelection = false;
    Dia::Core::StringCRC mVisited[kMaxVisited];
    unsigned int mVisitedCount = 0;
};

} } // namespace Cluiche::AppFlow
