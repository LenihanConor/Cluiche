#pragma once

#include <Types/StageCatalogue.h>
#include <Types/StageResult.h>

#include <cstdint>

namespace Cluiche { namespace AppFlow {

// One catalogue entry plus its last result, published MainPU -> RenderPU (catalogue-and-results AC 14).
// Plain fixed-size data so the whole snapshot is a bounded copy. The result has NO logPath (the menu does not need it;
// stages.results and the results file carry it). Tags are capped (kMaxTags x kTagChars) to keep the snapshot under
// kMaxSnapshotBytes; stages.get returns the full tag list.
struct StageCatalogueSnapshotEntry {
    static constexpr unsigned int kMaxTags = 8;
    static constexpr unsigned int kTagChars = 24;          // 23 characters + NUL, longer tags are truncated
    static constexpr unsigned int kDescriptionChars = 128; // matches String128
    static constexpr unsigned int kLabelChars = StageResult::kMaxLabel + 1;
    static constexpr unsigned int kSessionIdChars = 48;
    // Launch presets (names + descriptions only). Capped to keep the snapshot within budget; stages.get returns all.
    static constexpr unsigned int kMaxPresets = 4;
    static constexpr unsigned int kPresetNameChars = 32;          // matches String32
    static constexpr unsigned int kPresetDescriptionChars = 128;  // matches String128

    struct Preset {
        char name[kPresetNameChars] = {};
        char description[kPresetDescriptionChars] = {};
    };

    Dia::Core::StringCRC name;
    Dia::Game::StageKindMask kinds = 0;
    uint8_t tagCount = 0;
    char tags[kMaxTags][kTagChars] = {};
    char description[kDescriptionChars] = {};
    uint8_t presetCount = 0;
    Preset presets[kMaxPresets];

    StageResultSeverity severity = StageResultSeverity::kNone; // kNone = no result recorded
    char label[kLabelChars] = {};
    uint64_t utcMs = 0;
    char sessionId[kSessionIdChars] = {};
};

struct StageCatalogueSnapshot {
    static constexpr unsigned int kMaxEntries = StageCatalogue::kMaxStages;

    uint32_t revision = 0;
    uint32_t entryCount = 0;
    uint32_t omittedCount = 0;
    uint32_t droppedCount = 0;
    StageCatalogueSnapshotEntry entries[kMaxEntries];
};

static constexpr unsigned int kMaxStageCatalogueSnapshotBytes = 80 * 1024 /* was 48 KB; raised for launch presets */;
static_assert(sizeof(StageCatalogueSnapshot) <= kMaxStageCatalogueSnapshotBytes, "StageCatalogueSnapshot exceeds 80 KB");

}} // namespace Cluiche::AppFlow
