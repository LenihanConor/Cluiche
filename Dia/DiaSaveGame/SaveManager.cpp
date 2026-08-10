#include "DiaSaveGame/SaveManager.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include "DiaSaveGame/SaveRegistry.h"
#include "DiaSaveGame/ISaveable.h"
#include "DiaSaveGame/SaveContext.h"
#include "DiaSaveGame/LoadContext.h"
#include "DiaSaveGame/SaveManifest.h"

#include <stdio.h>
#include <string.h>

namespace Dia::SaveGame {

void SaveManager::Init(const SaveConfig& config, SaveRegistry& registry)
{
    mConfig   = &config;
    mRegistry = &registry;
    mSlotManager.Init(config);
}

void SaveManager::Shutdown()
{
    mConfig   = nullptr;
    mRegistry = nullptr;
}

SaveResult SaveManager::Save(Dia::Core::StringCRC slotId)
{
    DIA_ASSERT(mConfig   != nullptr, "SaveManager::Save called before Init");
    DIA_ASSERT(mRegistry != nullptr, "SaveManager::Save called before Init");

    if (mConfig->maxSlots > 0 && mSlotManager.IsMaxSlotsReached())
    {
        DIA_LOG_WARNING("savegame", "Save: slot capacity reached (%u)", mConfig->maxSlots);
        return SaveResult::Fail(SaveResultCode::SlotAtCapacity);
    }

    DIA_LOG_INFO("savegame", "Save: starting slot '%s'", slotId.AsChar());

    // Build manifest
    SaveManifest manifest;
    manifest.Build(*mRegistry, mConfig->format);

    // Serialize all participants + manifest into one SaveContext
    SaveContext ctx;
    manifest.Write(ctx);

    for (uint32_t i = 0; i < mRegistry->GetParticipantCount(); ++i)
    {
        ISaveable* p = mRegistry->GetParticipantAt(i);
        Dia::Core::StringCRC id = mRegistry->GetIdAt(i);
        ctx.BeginObject(id);
        p->Serialize(ctx);
        ctx.EndObject();
    }

    // Flush to buffer
    static char sBuffer[kBufferSize];
    if (!ctx.Flush(sBuffer, kBufferSize))
    {
        DIA_LOG_ERROR("savegame", "Save: serialize buffer overflow for slot '%s'", slotId.AsChar());
        return SaveResult::Fail(SaveResultCode::SerializeError);
    }

    // Write to disk
    char path[kMaxPathLen];
    mSlotManager.BuildPath(slotId, path, kMaxPathLen);

    FILE* f = fopen(path, "wb");
    if (!f)
    {
        DIA_LOG_ERROR("savegame", "Save: cannot open '%s' for writing", path);
        return SaveResult::Fail(SaveResultCode::FileWriteError);
    }
    const size_t len = strlen(sBuffer);
    const size_t written = fwrite(sBuffer, 1, len, f);
    fclose(f);

    if (written != len)
    {
        DIA_LOG_ERROR("savegame", "Save: write incomplete for '%s'", path);
        return SaveResult::Fail(SaveResultCode::FileWriteError);
    }

    DIA_LOG_INFO("savegame", "Save: completed slot '%s' (%zu bytes)", slotId.AsChar(), len);
    return SaveResult::Success();
}

LoadResult SaveManager::Load(Dia::Core::StringCRC slotId)
{
    DIA_ASSERT(mConfig   != nullptr, "SaveManager::Load called before Init");
    DIA_ASSERT(mRegistry != nullptr, "SaveManager::Load called before Init");

    DIA_LOG_INFO("savegame", "Load: starting slot '%s'", slotId.AsChar());

    // Check slot exists on disk
    if (!mSlotManager.SlotExists(slotId))
    {
        DIA_LOG_ERROR("savegame", "Load: slot '%s' not found", slotId.AsChar());
        return LoadResult::Fail(LoadResultCode::SlotNotFound);
    }

    // Read file
    char path[kMaxPathLen];
    mSlotManager.BuildPath(slotId, path, kMaxPathLen);

    static char sBuffer[kBufferSize];
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        DIA_LOG_ERROR("savegame", "Load: cannot open '%s'", path);
        return LoadResult::Fail(LoadResultCode::FileReadError);
    }
    const size_t read = fread(sBuffer, 1, kBufferSize - 1, f);
    fclose(f);
    sBuffer[read] = '\0';

    // Parse JSON
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(sBuffer, root))
    {
        DIA_LOG_ERROR("savegame", "Load: JSON parse error for slot '%s'", slotId.AsChar());
        return LoadResult::Fail(LoadResultCode::ParseError);
    }

    LoadContext ctx(root);

    // Read + validate manifest
    SaveManifest manifest;
    if (!manifest.Read(ctx))
    {
        DIA_LOG_ERROR("savegame", "Load: missing or corrupt manifest in slot '%s'", slotId.AsChar());
        return LoadResult::Fail(LoadResultCode::ParseError);
    }

    CompatResult compat = manifest.CheckCompatibility(*mRegistry);
    if (compat == CompatResult::EngineMismatch)
    {
        DIA_LOG_ERROR("savegame", "Load: engine version mismatch in slot '%s' (file=%u current=%u)",
            slotId.AsChar(), manifest.GetEngineVersion(), kEngineVersion);
        return LoadResult::Fail(LoadResultCode::EngineMismatch);
    }

    // Helper: find the saved version for a participant id by scanning the manifest.
    // Returns UINT32_MAX when the participant is not in the manifest (new participant — no migration needed).
    auto FindSavedVersion = [&manifest](Dia::Core::StringCRC id) -> uint32_t {
        for (uint32_t m = 0; m < manifest.GetParticipantCount(); ++m)
        {
            if (manifest.GetParticipantAt(m).id == id)
                return manifest.GetParticipantAt(m).version;
        }
        return UINT32_MAX;
    };

    // Deserialize participants in registration order, applying migrations as needed.
    for (uint32_t i = 0; i < mRegistry->GetParticipantCount(); ++i)
    {
        ISaveable* p = mRegistry->GetParticipantAt(i);
        Dia::Core::StringCRC id = mRegistry->GetIdAt(i);

        if (!root.isMember(id.AsChar()))
        {
            DIA_LOG_WARNING("savegame", "Load: participant '%s' not found in save — skipping", id.AsChar());
            continue;
        }

        // Wrap the participant's sub-object in a child LoadContext
        LoadContext pCtx(root[id.AsChar()]);

        // Apply migration chain when the saved version is behind the live version
        const uint32_t savedVersion = FindSavedVersion(id);
        const uint32_t liveVersion  = p->GetVersion();

        if (savedVersion != UINT32_MAX && savedVersion < liveVersion)
        {
            for (uint32_t fromVer = savedVersion; fromVer < liveVersion; ++fromVer)
            {
                if (!mRegistry->HasMigration(id, fromVer))
                {
                    DIA_LOG_ERROR("savegame",
                        "Load: missing migration for '%s' v%u->v%u — aborting",
                        id.AsChar(), fromVer, fromVer + 1);
                    return LoadResult::Fail(LoadResultCode::MigrationError);
                }

                DIA_LOG_INFO("savegame",
                    "Load: applying migration for '%s' v%u->v%u",
                    id.AsChar(), fromVer, fromVer + 1);

                mRegistry->ApplyMigration(id, fromVer, pCtx);
            }
        }

        p->Deserialize(pCtx);
    }

    DIA_LOG_INFO("savegame", "Load: completed slot '%s'", slotId.AsChar());
    return LoadResult::Success();
}

void SaveManager::DeleteSlot(Dia::Core::StringCRC slotId)
{
    DIA_ASSERT(mConfig != nullptr, "SaveManager not initialised");
    mSlotManager.DeleteSlot(slotId);
}

void SaveManager::RenameSlot(Dia::Core::StringCRC fromId, Dia::Core::StringCRC toId)
{
    DIA_ASSERT(mConfig != nullptr, "SaveManager not initialised");
    mSlotManager.RenameSlot(fromId, toId);
}

void SaveManager::EnumerateSlots(Dia::Core::Containers::DynamicArrayC<SlotInfo, 32>& out) const
{
    DIA_ASSERT(mConfig != nullptr, "SaveManager not initialised");
    mSlotManager.EnumerateSlots(out);
}

} // namespace Dia::SaveGame
