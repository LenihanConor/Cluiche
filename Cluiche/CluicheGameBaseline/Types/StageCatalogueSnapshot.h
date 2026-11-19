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

    Dia::Core::StringCRC name;
    Dia::Game::StageKindMask kinds = 0;
    uint8_t tagCount = 0;
    char tags[kMaxTags][kTagChars] = {};
    char description[kDescriptionChars] = {};

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

static constexpr unsigned int kMaxStageCatalogueSnapshotBytes = 48 * 1024;
static_assert(sizeof(StageCatalogueSnapshot) <= kMaxStageCatalogueSnapshotBytes, "StageCatalogueSnapshot exceeds 48 KB");

}} // namespace Cluiche::AppFlow
