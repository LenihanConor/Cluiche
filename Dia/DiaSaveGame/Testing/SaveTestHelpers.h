#pragma once

#include <DiaSaveGame/ISaveable.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <DiaSaveGame/SaveManifest.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveFormat.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>
#include <cstdio>

namespace Dia::SaveGame::Testing {

// ---------------------------------------------------------------------------
// MockSaveable
//
// Minimal ISaveable implementation used by unit tests to verify that
// SaveManager / SaveRegistry call Serialize / Deserialize the correct number
// of times and pass the correct data through.
// ---------------------------------------------------------------------------
struct MockSaveable : Dia::SaveGame::ISaveable
{
    int      serializeCount   = 0;
    int      deserializeCount = 0;
    int32_t  valueToWrite     = 0;
    int32_t  lastReadValue    = 0;
    uint32_t version          = 1;

    void Serialize(Dia::SaveGame::SaveContext& ctx) const override
    {
        // const_cast to increment counter (test helper, not production code)
        const_cast<MockSaveable*>(this)->serializeCount++;
        ctx.Write(Dia::Core::StringCRC("value"), valueToWrite);
    }

    void Deserialize(Dia::SaveGame::LoadContext& ctx) override
    {
        deserializeCount++;
        ctx.Read(Dia::Core::StringCRC("value"), lastReadValue);
    }

    uint32_t GetVersion() const override { return version; }
};

// ---------------------------------------------------------------------------
// InMemoryRoundTrip
//
// Performs a full Save/Load cycle without touching disk.
// Replicates the serialisation logic of SaveManager but flushes into an
// internal char buffer instead of a file.
//
// Usage:
//   InMemoryRoundTrip rt;
//   rt.Save(registry);
//   rt.Load(registry);  // participants are deserialised from the buffer
// ---------------------------------------------------------------------------
struct InMemoryRoundTrip
{
    static const unsigned int kBufferSize = 65536;

    char buffer[kBufferSize] = {};

    // Serialise all participants in reg into the internal buffer.
    // Returns false on overflow or other serialisation error.
    bool Save(Dia::SaveGame::SaveRegistry& reg,
              Dia::SaveGame::SaveFormat fmt = Dia::SaveGame::SaveFormat::Json)
    {
        Dia::SaveGame::SaveManifest manifest;
        manifest.Build(reg, fmt);

        Dia::SaveGame::SaveContext ctx;
        manifest.Write(ctx);

        for (uint32_t i = 0; i < reg.GetParticipantCount(); ++i)
        {
            Dia::SaveGame::ISaveable*  p  = reg.GetParticipantAt(i);
            Dia::Core::StringCRC       id = reg.GetIdAt(i);

            ctx.BeginObject(id);
            p->Serialize(ctx);
            ctx.EndObject();
        }

        return ctx.Flush(buffer, kBufferSize);
    }

    // Deserialise all participants in reg from the internal buffer.
    // Returns true on success, false on JSON parse error or missing manifest.
    bool Load(Dia::SaveGame::SaveRegistry& reg)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(buffer, root))
            return false;

        Dia::SaveGame::LoadContext ctx(root);

        Dia::SaveGame::SaveManifest manifest;
        if (!manifest.Read(ctx))
            return false;

        for (uint32_t i = 0; i < reg.GetParticipantCount(); ++i)
        {
            Dia::SaveGame::ISaveable*  p  = reg.GetParticipantAt(i);
            Dia::Core::StringCRC       id = reg.GetIdAt(i);

            if (!root.isMember(id.AsChar()))
                continue;

            Dia::SaveGame::LoadContext pCtx(root[id.AsChar()]);
            p->Deserialize(pCtx);
        }

        return true;
    }
};

// ---------------------------------------------------------------------------
// AssertSlotExists
//
// Returns true if a save file exists at the path produced by
// config.baseDirectory + replacing {id} in config.slotPattern with the
// string representation of slotId.
// ---------------------------------------------------------------------------
inline bool AssertSlotExists(const Dia::SaveGame::SaveConfig& config,
                             Dia::Core::StringCRC slotId)
{
    // Replicate SlotManager::BuildPath logic without depending on SlotManager
    char path[512];

    const char* pattern     = config.slotPattern;
    const char* placeholder = ::strstr(pattern, "{id}");

    if (placeholder != nullptr)
    {
        const ptrdiff_t prefixLen = placeholder - pattern;
        ::snprintf(path, sizeof(path), "%s%.*s%s%s",
            config.baseDirectory,
            static_cast<int>(prefixLen), pattern,
            slotId.AsChar(),
            placeholder + 4); // skip past "{id}"
    }
    else
    {
        ::snprintf(path, sizeof(path), "%s%s",
            config.baseDirectory, pattern);
    }

    FILE* f = ::fopen(path, "rb");
    if (!f)
        return false;
    ::fclose(f);
    return true;
}

// TODO (Task 9 - Observer events): Add AssertEventFired helper once
// SaveManager observer / event dispatch is implemented.

} // namespace Dia::SaveGame::Testing
