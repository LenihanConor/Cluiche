#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String256.h>
#include <DiaGame/StageKinds.h>

namespace Cluiche { namespace AppFlow {

// One stage as listed by the catalogue. Names, tags and kinds are StringCRC (PD-001; StringCRC keeps its text for display).
struct StageCatalogueEntry {
    static constexpr unsigned int kMaxTags = 16;

    Dia::Core::StringCRC name;
    // Tags exactly as written in the .diastage (order preserved).
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxTags> tags;
    // Canonical kinds present in the tags (showcase, tool, test bits). 0 = no kind (listed under "other").
    Dia::Game::StageKindMask kinds = 0;
    Dia::Core::Containers::String128 description;   // empty when absent
    Dia::Core::Containers::String256 diastagePath;  // empty when the stage has no .diastage import
};

// Group order used by GroupByKind.
enum class StageGroup : uint8_t { kShowcase, kTool, kTest, kOther };
static const unsigned int kStageGroupCount = 4;
const char* StageGroupName(StageGroup group);

struct StageCatalogueRow {
    StageGroup group = StageGroup::kOther;
    unsigned int entryIndex = 0;
};

// An entry has at most 3 kinds, so rows <= 3 * kMaxStages.
using StageCatalogueRows = Dia::Core::Containers::DynamicArrayC<StageCatalogueRow, 192>;
using StageCatalogueIndices = Dia::Core::Containers::DynamicArrayC<unsigned int, 64>;

class StageCatalogue {
public:
    static constexpr unsigned int kMaxStages = 64;

    void Clear();
    // False (entry not stored) when already at kMaxStages; the loader counts those as dropped.
    bool Add(const StageCatalogueEntry& entry);

    unsigned int Size() const { return mEntries.Size(); }
    const StageCatalogueEntry& operator[](unsigned int index) const { return mEntries[index]; }

    unsigned int OmittedCount() const { return mOmittedCount; }
    unsigned int DroppedCount() const { return mDroppedCount; }
    void SetOmittedCount(unsigned int n) { mOmittedCount = n; }
    void SetDroppedCount(unsigned int n) { mDroppedCount = n; }

    // Index of the entry with this name, or -1.
    int Find(const Dia::Core::StringCRC& name) const;
    // Kind bitmask of the entry at index (0 if out of range).
    Dia::Game::StageKindMask Kinds(unsigned int entryIndex) const;
    // Indices of entries carrying every tag (exact match). n == 0 returns all entries.
    void Filter(const Dia::Core::StringCRC* tags, unsigned int n, StageCatalogueIndices& out) const;
    // Rows {group, entryIndex} in group order showcase, tool, test, other; ascending entry index within a group.
    // One row per kind the entry has; entries with no kind get one "other" row.
    void GroupByKind(StageCatalogueRows& out) const;

private:
    Dia::Core::Containers::DynamicArrayC<StageCatalogueEntry, kMaxStages> mEntries;
    unsigned int mOmittedCount = 0;
    unsigned int mDroppedCount = 0;
};

}} // namespace Cluiche::AppFlow
