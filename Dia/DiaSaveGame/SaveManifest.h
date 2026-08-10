#pragma once

#include <stdint.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "DiaSaveGame/SaveFormat.h"

namespace Dia::SaveGame {

class SaveContext;
class LoadContext;
class SaveRegistry;

// Current engine save-format version. Bump when the manifest schema changes.
static const uint32_t kEngineVersion = 1;

// Per-participant record stored in the manifest.
struct ManifestParticipant {
    Dia::Core::StringCRC id;
    uint32_t             version;
};

// CompatibilityResult returned by CheckCompatibility.
enum class CompatResult : uint8_t {
    Ok,                  // all participants match; safe to load
    EngineMismatch,      // manifest engine version != kEngineVersion; abort load
    ParticipantMigration // one or more participants have a version delta; run migrations
};

// Header written at the start of every save slot.
// Owns no heap; all strings use StringCRC (capped at 64 chars).
class SaveManifest {
public:
    static const unsigned int kMaxParticipants = 32;

    SaveManifest();

    // Build a manifest from the live SaveRegistry (call before Save).
    void Build(const SaveRegistry& registry, SaveFormat format);

    // Serialize to / deserialize from a SaveContext / LoadContext.
    void Write(SaveContext& ctx) const;
    bool Read(LoadContext& ctx);

    // Compare this manifest (loaded from disk) against the live registry.
    // Returns EngineMismatch if engine versions differ.
    // Returns ParticipantMigration if any participant version differs.
    // Returns Ok when everything matches.
    CompatResult CheckCompatibility(const SaveRegistry& registry) const;

    // Accessors used by SaveManager during load.
    uint32_t       GetEngineVersion   () const { return mEngineVersion; }
    SaveFormat     GetFormat          () const { return mFormat; }
    uint32_t       GetTimestamp       () const { return mTimestamp; }
    uint32_t       GetParticipantCount() const;
    const ManifestParticipant& GetParticipantAt(uint32_t index) const;

    void SetTimestamp(uint32_t ts) { mTimestamp = ts; }

private:
    uint32_t    mEngineVersion;
    SaveFormat  mFormat;
    uint32_t    mTimestamp;   // Unix seconds; set by caller (SaveManager)
    Dia::Core::Containers::DynamicArrayC<ManifestParticipant, kMaxParticipants> mParticipants;
};

} // namespace Dia::SaveGame
