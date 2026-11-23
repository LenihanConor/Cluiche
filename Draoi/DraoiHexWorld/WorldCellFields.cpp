#include <DraoiHexWorld/WorldCellFields.h>

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Draoi {
namespace HexWorld {

const WorldCellFields::Entry* WorldCellFields::Find(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
{
    for (uint32_t i = 0; i < mEntries.Size(); ++i)
    {
        const Entry& e = mEntries[i];
        if (e.slot == slot && e.field == field)
            return &e;
    }
    return nullptr;
}

void WorldCellFields::CheckReadsAllowed() const
{
    DIA_ASSERT(mReadsForbidden == nullptr || !*mReadsForbidden, "WorldCellFields read while reads are forbidden");
}

void WorldCellFields::Register(Dia::Core::StringCRC slot, Dia::Core::StringCRC field, const FieldReader& reader)
{
    for (uint32_t i = 0; i < mEntries.Size(); ++i)
    {
        if (mEntries[i].slot == slot && mEntries[i].field == field)
        {
            mEntries[i].reader = reader;
            return;
        }
    }
    DIA_ASSERT(!mEntries.IsFull(), "WorldCellFields capacity exceeded");
    if (mEntries.IsFull())
    {
        DIA_LOG_ERROR("HexWorld", "WorldCellFields: registry full (%u fields); field dropped", kMaxFields);
        return;
    }
    Entry e;
    e.slot = slot;
    e.field = field;
    e.reader = reader;
    mEntries.Add(e);
}

void WorldCellFields::Clear()
{
    mEntries.RemoveAll();
}

float WorldCellFields::GetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
{
    CheckReadsAllowed();
    const Entry* e = Find(slot, field);
    return (e && e->reader.getFloat) ? e->reader.getFloat(e->reader.ctx, mCell) : 0.0f;
}

bool WorldCellFields::GetBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
{
    CheckReadsAllowed();
    const Entry* e = Find(slot, field);
    return (e && e->reader.getBool) ? e->reader.getBool(e->reader.ctx, mCell) : false;
}

bool WorldCellFields::Has(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
{
    return Find(slot, field) != nullptr;
}

} // namespace HexWorld
} // namespace Draoi
