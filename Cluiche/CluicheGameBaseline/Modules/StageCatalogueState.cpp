#include "Modules/StageCatalogueState.h"

#include <cstring>
#include <filesystem>

namespace Cluiche { namespace AppFlow {

namespace {

void CopyBounded(char* dst, unsigned int cap, const char* src)
{
    if (cap == 0) { return; }
    unsigned int n = 0;
    if (src != nullptr)
    {
        while (src[n] != '\0' && n + 1 < cap) { dst[n] = src[n]; ++n; }
    }
    dst[n] = '\0';
}

} // namespace

StageCatalogueState::StageCatalogueState()
    : mCatalogue(new StageCatalogue())
    , mResults(new StageResultsTable())
{
}

StageCatalogueResult StageCatalogueState::LoadCatalogue(const char* diagamePath)
{
    mCatalogue->Clear();
    return StageCatalogueLoader::Load(diagamePath, *mCatalogue);
}

StageResultsReadResult StageCatalogueState::LoadResults(const char* path)
{
    mResults->Clear();
    return StageResultsFile::Read(path, *mResults);
}

bool StageCatalogueState::ApplyEvent(const StageResultEvent& event, const char* sessionId, const char* logPath)
{
    StageResultEvent filled = event;
    if (filled.result.sessionId.IsEmpty()) { filled.result.SetSessionId(sessionId); }
    if (filled.result.logPath.IsEmpty()) { filled.result.SetLogPath(logPath); }
    if (!mResults->Set(filled)) { return false; }
    ++mRevision;
    mDirty = true;
    return true;
}

StageCatalogueState::FlushResult StageCatalogueState::FlushFrame()
{
    if (!mDirty || !HasPersistence()) { mDirty = false; return FlushResult::kNothingToWrite; }
    // A failed write is not retried every frame: the next event marks the table dirty again and rewrites the merge.
    mDirty = false;
    ++mWriteCount;
    if (StageResultsFile::Write(mResultsPath.c_str(), *mResults))
    {
        mLastWriteFailed = false;
        return FlushResult::kWritten;
    }
    mLastWriteFailed = true;
    return FlushResult::kFailed;
}

void StageCatalogueState::BuildSnapshot(StageCatalogueSnapshot& out) const
{
    out.revision = mRevision;
    out.omittedCount = mCatalogue->OmittedCount();
    out.droppedCount = mCatalogue->DroppedCount();
    const unsigned int count = mCatalogue->Size() < StageCatalogueSnapshot::kMaxEntries ? mCatalogue->Size() : StageCatalogueSnapshot::kMaxEntries;
    out.entryCount = count;
    for (unsigned int i = 0; i < count; ++i)
    {
        const StageCatalogueEntry& src = (*mCatalogue)[i];
        StageCatalogueSnapshotEntry& dst = out.entries[i];
        dst = StageCatalogueSnapshotEntry();
        dst.name = src.name;
        dst.kinds = src.kinds;
        const unsigned int tagCount = src.tags.Size() < StageCatalogueSnapshotEntry::kMaxTags ? src.tags.Size() : StageCatalogueSnapshotEntry::kMaxTags;
        dst.tagCount = static_cast<uint8_t>(tagCount);
        for (unsigned int t = 0; t < tagCount; ++t)
        {
            CopyBounded(dst.tags[t], StageCatalogueSnapshotEntry::kTagChars, src.tags[t].AsChar());
        }
        CopyBounded(dst.description, StageCatalogueSnapshotEntry::kDescriptionChars, src.description.AsCStr());
        const unsigned int presetCount = src.presets.Size() < StageCatalogueSnapshotEntry::kMaxPresets ? src.presets.Size() : StageCatalogueSnapshotEntry::kMaxPresets;
        dst.presetCount = static_cast<uint8_t>(presetCount);
        for (unsigned int p = 0; p < presetCount; ++p)
        {
            CopyBounded(dst.presets[p].name, StageCatalogueSnapshotEntry::kPresetNameChars, src.presets[p].name.AsCStr());
            CopyBounded(dst.presets[p].description, StageCatalogueSnapshotEntry::kPresetDescriptionChars, src.presets[p].description.AsCStr());
        }

        const int r = mResults->Find(src.name);
        if (r >= 0)
        {
            const StageResult& res = (*mResults)[static_cast<unsigned int>(r)].result;
            dst.severity = res.severity;
            CopyBounded(dst.label, StageCatalogueSnapshotEntry::kLabelChars, res.label.AsCStr());
            dst.utcMs = res.utcMs;
            CopyBounded(dst.sessionId, StageCatalogueSnapshotEntry::kSessionIdChars, res.sessionId.AsCStr());
        }
    }
}

std::string DeriveResultsPathFromSessionDir(const char* sessionDir)
{
    if (sessionDir == nullptr || sessionDir[0] == '\0') { return std::string(); }
    namespace fs = std::filesystem;
    fs::path dir(sessionDir);
    if (!dir.has_filename()) { dir = dir.parent_path(); }   // strip trailing separator
    const fs::path sessions = dir.parent_path();
    if (sessions.filename() != "sessions") { return std::string(); }
    return (sessions.parent_path() / "stage_results.json").string();
}

}} // namespace Cluiche::AppFlow
