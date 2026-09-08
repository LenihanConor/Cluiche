#include "DiaSaveGame/SaveRegistry.h"

#include <DiaCore/Core/Assert.h>

namespace Dia::SaveGame {

void SaveRegistry::Register(Dia::Core::StringCRC id, ISaveable* participant)
{
    DIA_ASSERT(participant != nullptr, "SaveRegistry::Register - participant must not be null");

    for (unsigned int i = 0; i < mParticipants.Size(); ++i)
    {
        DIA_ASSERT(mParticipants.At(i).id != id,
            "SaveRegistry::Register - id '%s' is already registered", id.AsChar());
    }

    DIA_ASSERT(!mParticipants.IsFull(),
        "SaveRegistry::Register - participant list is full (capacity %u)", kMaxParticipants);

    ParticipantEntry entry;
    entry.id          = id;
    entry.participant = participant;
    mParticipants.Add(entry);
}

void SaveRegistry::RegisterMigration(Dia::Core::StringCRC id, uint32_t fromVersion, MigrationFn fn)
{
    DIA_ASSERT(fn, "SaveRegistry::RegisterMigration - migration function must not be null");

    DIA_ASSERT(!mMigrations.IsFull(),
        "SaveRegistry::RegisterMigration - migration list is full (capacity %u)", kMaxMigrations);

    MigrationEntry entry;
    entry.id          = id;
    entry.fromVersion = fromVersion;
    entry.fn          = fn;
    mMigrations.Add(entry);
}

void SaveRegistry::Unregister(Dia::Core::StringCRC id)
{
    for (unsigned int i = 0; i < mParticipants.Size(); ++i)
    {
        if (mParticipants.At(i).id == id)
        {
            mParticipants.RemoveAt(i);
            return;
        }
    }

    DIA_ASSERT(false, "SaveRegistry::Unregister - id '%s' was not registered", id.AsChar());
}

uint32_t SaveRegistry::GetParticipantCount() const
{
    return static_cast<uint32_t>(mParticipants.Size());
}

ISaveable* SaveRegistry::GetParticipantAt(uint32_t index) const
{
    DIA_ASSERT(index < mParticipants.Size(),
        "SaveRegistry::GetParticipantAt - index %u out of range (size %u)", index, mParticipants.Size());
    return mParticipants.At(index).participant;
}

Dia::Core::StringCRC SaveRegistry::GetIdAt(uint32_t index) const
{
    DIA_ASSERT(index < mParticipants.Size(),
        "SaveRegistry::GetIdAt - index %u out of range (size %u)", index, mParticipants.Size());
    return mParticipants.At(index).id;
}

bool SaveRegistry::HasMigration(Dia::Core::StringCRC id, uint32_t fromVersion) const
{
    for (unsigned int i = 0; i < mMigrations.Size(); ++i)
    {
        const MigrationEntry& entry = mMigrations.At(i);
        if (entry.id == id && entry.fromVersion == fromVersion)
            return true;
    }
    return false;
}

void SaveRegistry::ApplyMigration(Dia::Core::StringCRC id, uint32_t fromVersion, LoadContext& ctx) const
{
    for (unsigned int i = 0; i < mMigrations.Size(); ++i)
    {
        const MigrationEntry& entry = mMigrations.At(i);
        if (entry.id == id && entry.fromVersion == fromVersion)
        {
            entry.fn(ctx);
            return;
        }
    }

    DIA_ASSERT(false,
        "SaveRegistry::ApplyMigration - no migration found for id '%s' fromVersion %u",
        id.AsChar(), fromVersion);
}

} // namespace Dia::SaveGame
