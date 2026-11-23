// Per-world registry of per-cell fields keyed slot.field; doubles as a DiaCondition context with a cell cursor.
// Spec: World-Sim Kit, Draoi half (WorldCellFields). Thread-neutral: reads touch committed state only.
#pragma once

#include <DiaCondition/IConditionContext.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Draoi {
namespace HexWorld {

// Typed reader over a cell index. A field supplies getFloat, getBool, or both; a missing getter reads as 0/false.
struct FieldReader
{
    float (*getFloat)(const void* ctx, uint32_t cell) = nullptr;
    bool  (*getBool)(const void* ctx, uint32_t cell)  = nullptr;
    const void* ctx = nullptr;
};

class WorldCellFields : public Dia::Condition::IConditionContext
{
public:
    static constexpr uint32_t kMaxFields = 128;

    // Registers (or replaces: last registration wins) the reader for slot.field.
    void Register(Dia::Core::StringCRC slot, Dia::Core::StringCRC field, const FieldReader& reader);
    void Clear();

    // Optional: when *flag is true at read time, reads assert in Debug. Pass nullptr to disable.
    void SetReadsForbiddenFlag(const bool* flag) { mReadsForbidden = flag; }

    void     SetCell(uint32_t cell) { mCell = cell; }
    uint32_t GetCell() const { return mCell; }

    float GetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override;
    bool  GetBool (Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override;
    bool  Has     (Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override;

private:
    struct Entry
    {
        Dia::Core::StringCRC slot;
        Dia::Core::StringCRC field;
        FieldReader          reader;
    };

    const Entry* Find(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const;
    void         CheckReadsAllowed() const;

    Dia::Core::Containers::DynamicArrayC<Entry, kMaxFields> mEntries;
    const bool* mReadsForbidden = nullptr;
    uint32_t    mCell = 0;
};

} // namespace HexWorld
} // namespace Draoi
