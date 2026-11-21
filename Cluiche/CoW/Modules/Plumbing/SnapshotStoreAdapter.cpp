#include "Modules/SnapshotStoreAdapter.h"
#include "Modules/SaveLoadModule.h"

#include <DiaSaveGame/SlotManager.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace CoW {

bool SnapshotStoreAdapter::IsValidName(const char* name)
{
    if (name == nullptr || name[0] == '\0')
        return false;
    const size_t len = std::strlen(name);
    if (len > kMaxNameLength)
        return false;
    for (size_t i = 0; i < len; ++i)
    {
        const char c = name[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok)
            return false;
    }
    return Dia::SaveGame::SlotManager::IsValidSlotId(name);   // also rejects CON, NUL, COM1...
}

void SnapshotStoreAdapter::Fire(const Request& r, bool ok, const char* message)
{
    if (r.cb != nullptr)
        r.cb(ok, message, r.user);
}

void SnapshotStoreAdapter::Queue(Kind kind, const char* name, Dia::CellHistory::SnapshotCallback cb, void* user)
{
    Request r;
    r.kind = kind;
    r.cb = cb;
    r.user = user;
    r.nameValid = IsValidName(name);
    if (r.nameValid)
        std::snprintf(r.name, sizeof(r.name), "%s", name);

    if (!r.nameValid || mHasPending)
    {
        if (mRejectedCount < sizeof(mRejected) / sizeof(mRejected[0]))
            mRejected[mRejectedCount++] = r;
        else
            Fire(r, false, "busy: too many snapshot requests");   // never drop a callback
        return;
    }
    mPending = r;
    mHasPending = true;
    mDeferredFrames = 0;
}

bool SnapshotStoreAdapter::Exists(const char* name) const
{
    if (!IsValidName(name))
        return false;
    char path[SaveLoadModule::kSnapshotPathCapacity];
    mOwner.BuildSnapshotPath(name, path, sizeof(path));
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool SnapshotStoreAdapter::Delete(const char* name)
{
    if (!IsValidName(name))
        return false;
    char path[SaveLoadModule::kSnapshotPathCapacity];
    mOwner.BuildSnapshotPath(name, path, sizeof(path));
    std::error_code ec;
    return std::filesystem::remove(path, ec) && !ec;
}

const char* SnapshotStoreAdapter::Folder() const
{
    return mOwner.GetSnapshotFolder();
}

void SnapshotStoreAdapter::FailAll(const char* reason)
{
    Request all[1 + sizeof(mRejected) / sizeof(mRejected[0])];
    uint32_t count = 0;
    if (mHasPending)
        all[count++] = mPending;
    for (uint32_t i = 0; i < mRejectedCount; ++i)
        all[count++] = mRejected[i];
    mHasPending = false;
    mRejectedCount = 0;
    mDeferredFrames = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        mOwner.NoteSnapshotFailure(all[i].kind == Kind::Take, all[i].name, reason);
        Fire(all[i], false, reason);
    }
}

void SnapshotStoreAdapter::RunFrameBoundary()
{
    if (mHasPending)
    {
        if (mOwner.HasPendingSave() || mOwner.HasPendingLoad())
        {
            // Never overwrite the user's request: wait behind it.
            if (++mDeferredFrames > kDeferLimitFrames)
            {
                const Request r = mPending;
                mHasPending = false;
                mDeferredFrames = 0;
                mOwner.NoteSnapshotFailure(r.kind == Kind::Take, r.name, "deferred too long behind a pending save or load");
                Fire(r, false, "deferred too long behind a pending save or load");
            }
        }
        else
        {
            const Request r = mPending;
            mHasPending = false;
            mDeferredFrames = 0;
            char message[192] = {};
            const bool ok = (r.kind == Kind::Take) ? mOwner.SnapshotSave(r.name, message, sizeof(message))
                                                   : mOwner.SnapshotLoad(r.name, message, sizeof(message));
            Fire(r, ok, message);
        }
    }

    if (mRejectedCount > 0)
    {
        Request rejected[sizeof(mRejected) / sizeof(mRejected[0])];
        const uint32_t count = mRejectedCount;
        for (uint32_t i = 0; i < count; ++i)
            rejected[i] = mRejected[i];
        mRejectedCount = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const char* msg = rejected[i].nameValid ? "busy: another snapshot request is pending" : "invalid snapshot name";
            mOwner.NoteSnapshotFailure(rejected[i].kind == Kind::Take, rejected[i].name, msg);
            Fire(rejected[i], false, msg);
        }
    }
}

} // namespace CoW
