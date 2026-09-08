#include "DiaSaveGame/SaveManifest.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>

#include "DiaSaveGame/SaveContext.h"
#include "DiaSaveGame/LoadContext.h"
#include "DiaSaveGame/SaveRegistry.h"

namespace Dia::SaveGame {

namespace {
    // Keys used when serialising the manifest block.
    const Dia::Core::StringCRC kKeyManifest      ("manifest");
    const Dia::Core::StringCRC kKeyEngineVersion ("engine_version");
    const Dia::Core::StringCRC kKeyFormat        ("format");
    const Dia::Core::StringCRC kKeyTimestamp     ("timestamp");
    const Dia::Core::StringCRC kKeyParticipants  ("participants");
    const Dia::Core::StringCRC kKeyId            ("id");
    const Dia::Core::StringCRC kKeyVersion       ("version");
}

SaveManifest::SaveManifest()
    : mEngineVersion(kEngineVersion)
    , mFormat(SaveFormat::Json)
    , mTimestamp(0)
{
}

void SaveManifest::Build(const SaveRegistry& registry, SaveFormat format)
{
    mEngineVersion = kEngineVersion;
    mFormat        = format;
    mParticipants.RemoveAll();

    for (uint32_t i = 0; i < registry.GetParticipantCount() && !mParticipants.IsFull(); ++i)
    {
        ManifestParticipant entry;
        entry.id      = registry.GetIdAt(i);
        entry.version = registry.GetParticipantAt(i)->GetVersion();
        mParticipants.Add(entry);
    }
}

void SaveManifest::Write(SaveContext& ctx) const
{
    ctx.BeginObject(kKeyManifest);
        ctx.Write(kKeyEngineVersion, static_cast<int32_t>(mEngineVersion));
        ctx.Write(kKeyFormat,        static_cast<int32_t>(static_cast<int>(mFormat)));
        ctx.Write(kKeyTimestamp,     static_cast<int32_t>(mTimestamp));

        ctx.BeginArray(kKeyParticipants);
        Json::Value& arr = ctx.CurrentNode();
        for (uint32_t i = 0; i < mParticipants.Size(); ++i)
        {
            Json::Value entry(Json::objectValue);
            entry[kKeyId.AsChar()]      = mParticipants[i].id.AsChar();
            entry[kKeyVersion.AsChar()] = static_cast<Json::Int>(mParticipants[i].version);
            arr.append(entry);
        }
        ctx.EndArray();
    ctx.EndObject();
}

bool SaveManifest::Read(LoadContext& ctx)
{
    if (!ctx.BeginObject(kKeyManifest))
        return false;

    int32_t ev = 0, fmt = 0, ts = 0;
    if (!ctx.Read(kKeyEngineVersion, ev))  { ctx.EndObject(); return false; }
    if (!ctx.Read(kKeyFormat,        fmt)) { ctx.EndObject(); return false; }
    if (!ctx.Read(kKeyTimestamp,     ts))  { ctx.EndObject(); return false; }

    mEngineVersion = static_cast<uint32_t>(ev);
    mFormat        = static_cast<SaveFormat>(fmt);
    mTimestamp     = static_cast<uint32_t>(ts);
    mParticipants.RemoveAll();

    uint32_t count = 0;
    if (ctx.BeginArray(kKeyParticipants, count))
    {
        const Json::Value& arr = ctx.CurrentNode();
        for (uint32_t i = 0; i < count && !mParticipants.IsFull(); ++i)
        {
            const Json::Value& entry = arr[i];
            if (!entry.isObject()) continue;
            if (!entry.isMember(kKeyId.AsChar()) || !entry.isMember(kKeyVersion.AsChar())) continue;

            ManifestParticipant p;
            p.id      = entry[kKeyId.AsChar()].asCString();
            p.version = static_cast<uint32_t>(entry[kKeyVersion.AsChar()].asInt());
            mParticipants.Add(p);
        }
        ctx.EndArray();
    }

    ctx.EndObject();
    return true;
}

CompatResult SaveManifest::CheckCompatibility(const SaveRegistry& registry) const
{
    if (mEngineVersion != kEngineVersion)
        return CompatResult::EngineMismatch;

    for (uint32_t i = 0; i < mParticipants.Size(); ++i)
    {
        const ManifestParticipant& mp = mParticipants[i];
        // Find matching entry in live registry
        for (uint32_t j = 0; j < registry.GetParticipantCount(); ++j)
        {
            if (registry.GetIdAt(j) == mp.id)
            {
                if (registry.GetParticipantAt(j)->GetVersion() != mp.version)
                    return CompatResult::ParticipantMigration;
                break;
            }
        }
    }

    return CompatResult::Ok;
}

uint32_t SaveManifest::GetParticipantCount() const
{
    return mParticipants.Size();
}

const ManifestParticipant& SaveManifest::GetParticipantAt(uint32_t index) const
{
    DIA_ASSERT(index < mParticipants.Size(), "SaveManifest::GetParticipantAt out of bounds");
    return mParticipants[index];
}

} // namespace Dia::SaveGame
