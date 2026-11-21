#pragma once

#include <Modules/StageCatalogueLoader.h>
#include <Modules/StageResultsFile.h>
#include <Types/StageCatalogueSnapshot.h>

#include <memory>
#include <string>

namespace Cluiche { namespace AppFlow {

// Pure logic behind StageCatalogueModule (no streams, no module machinery): the catalogue, the results table, the revision,
// write-once-per-frame and the snapshot build. Heap-allocates the catalogue (it is large).
class StageCatalogueState {
public:
    enum class FlushResult : uint8_t { kNothingToWrite, kWritten, kFailed };

    StageCatalogueState();

    // Loads the catalogue from a .diagame. On any error the catalogue stays empty (the module never fails the app).
    StageCatalogueResult LoadCatalogue(const char* diagamePath);
    // Reads the results file into the table. Any non-Ok result leaves the table empty.
    StageResultsReadResult LoadResults(const char* path);

    // Empty path = no persistence.
    void SetResultsPath(const char* path) { mResultsPath = path ? path : ""; }
    const char* ResultsPath() const { return mResultsPath.c_str(); }
    bool HasPersistence() const { return !mResultsPath.empty(); }

    // Stores the result (empty sessionId / logPath are filled from the arguments), bumps the revision and marks the
    // table dirty. False (not stored) when a new stage would exceed the table capacity or the stage name is empty.
    bool ApplyEvent(const StageResultEvent& event, const char* sessionId, const char* logPath);

    // Call once per frame after applying events: at most one StageResultsFile::Write for any number of events.
    FlushResult FlushFrame();

    uint32_t Revision() const { return mRevision; }
    uint32_t WriteCount() const { return mWriteCount; }
    bool LastWriteFailed() const { return mLastWriteFailed; }

    // True when the revision differs from the last published one (always true before the first publish).
    bool NeedsPublish() const { return !mPublishedAny || mPublishedRevision != mRevision; }
    void MarkPublished() { mPublishedAny = true; mPublishedRevision = mRevision; }
    void BuildSnapshot(StageCatalogueSnapshot& out) const;

    const StageCatalogue& Catalogue() const { return *mCatalogue; }
    const StageResultsTable& Results() const { return *mResults; }

private:
    std::unique_ptr<StageCatalogue> mCatalogue;
    std::unique_ptr<StageResultsTable> mResults;
    std::string mResultsPath;
    uint32_t mRevision = 0;
    uint32_t mPublishedRevision = 0;
    bool mPublishedAny = false;
    bool mDirty = false;
    bool mLastWriteFailed = false;
    uint32_t mWriteCount = 0;
};

// Results path next to the session folder: <sessionDir>/../../stage_results.json when the session dir's parent is named
// "sessions" (out/<App>/sessions/<id>/ -> out/<App>/stage_results.json). Empty when sessionDir does not have that shape.
std::string DeriveResultsPathFromSessionDir(const char* sessionDir);

}} // namespace Cluiche::AppFlow
