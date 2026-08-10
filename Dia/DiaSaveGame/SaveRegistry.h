#pragma once

#include <stdint.h>
#include <functional>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "DiaSaveGame/ISaveable.h"

namespace Dia::SaveGame {

class LoadContext;

using MigrationFn = std::function<void(LoadContext&)>;

class SaveRegistry {
public:
    static const unsigned int kMaxParticipants = 32;
    static const unsigned int kMaxMigrations   = 64;

    SaveRegistry() = default;

    void Register           (Dia::Core::StringCRC id, ISaveable* participant);
    void RegisterMigration  (Dia::Core::StringCRC id, uint32_t fromVersion, MigrationFn fn);
    void Unregister         (Dia::Core::StringCRC id);

    uint32_t                GetParticipantCount () const;
    ISaveable*              GetParticipantAt    (uint32_t index) const;
    Dia::Core::StringCRC    GetIdAt             (uint32_t index) const;

    bool HasMigration   (Dia::Core::StringCRC id, uint32_t fromVersion) const;
    void ApplyMigration (Dia::Core::StringCRC id, uint32_t fromVersion, LoadContext& ctx) const;

private:
    struct ParticipantEntry {
        Dia::Core::StringCRC id;
        ISaveable*           participant;
    };

    struct MigrationEntry {
        Dia::Core::StringCRC id;
        uint32_t             fromVersion;
        MigrationFn          fn;
    };

    Dia::Core::Containers::DynamicArrayC<ParticipantEntry, kMaxParticipants> mParticipants;
    Dia::Core::Containers::DynamicArrayC<MigrationEntry,   kMaxMigrations>   mMigrations;
};

} // namespace Dia::SaveGame
