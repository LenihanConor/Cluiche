#include <DraoiHexWorld/WorldCellFields.h>

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cmath>

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
    if (!e) return 0.0f;
    if (e->reader.getFloat) return e->reader.getFloat(e->reader.ctx, mCell);
    if (e->reader.getBool)  return e->reader.getBool(e->reader.ctx, mCell) ? 1.0f : 0.0f;   // bool-only fallback
    return 0.0f;
}

bool WorldCellFields::GetBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
{
    CheckReadsAllowed();
    const Entry* e = Find(slot, field);
    if (!e) return false;
    if (e->reader.getBool)  return e->reader.getBool(e->reader.ctx, mCell);
    if (e->reader.getFloat)   // float-only fallback: non-zero is true, NaN is false
    {
        const float v = e->reader.getFloat(e->reader.ctx, mCell);
        return v != 0.0f && !std::isnan(v);
    }
    return false;
}

bool WorldCellFields::Has(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const
{
    return Find(slot, field) != nullptr;
}

} // namespace HexWorld
} // namespace Draoi
