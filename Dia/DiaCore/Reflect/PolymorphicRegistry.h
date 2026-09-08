#pragma once
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia::Reflect {

// Forward declarations for archive types used in function pointers
class JsonWriteArchive;
class JsonReadArchive;
class BinaryWriteArchive;
class BinaryReadArchive;

// ---------------------------------------------------------
// PolymorphicEntry — one registered concrete type
// ---------------------------------------------------------
struct PolymorphicEntry {
    uint32_t    typeCrc;
    const char* typeName;
    void* (*factory)();
    void (*writeJson)(void* obj, JsonWriteArchive& ar);
    void (*readJson)(void* obj, JsonReadArchive& ar);
    void (*writeBinary)(void* obj, BinaryWriteArchive& ar);
    void (*readBinary)(void* obj, BinaryReadArchive& ar);
};

// ---------------------------------------------------------
// PolymorphicRegistry — singleton mapping type CRC to entry
// ---------------------------------------------------------
class PolymorphicRegistry {
public:
    static constexpr unsigned int kMaxTypes = 64u;

    static PolymorphicRegistry& Instance() {
        static PolymorphicRegistry instance;
        return instance;
    }

    void Register(const PolymorphicEntry& entry) {
        if (!mEntries.IsFull()) {
            mEntries.Add(entry);
        }
    }

    const PolymorphicEntry* Find(uint32_t crc) const {
        for (unsigned int i = 0u; i < mEntries.Size(); ++i) {
            if (mEntries.At(i).typeCrc == crc) {
                return &mEntries.At(i);
            }
        }
        return nullptr;
    }

    const PolymorphicEntry* FindByName(const char* name) const {
        Dia::Core::StringCRC nameCrc(name);
        return Find(nameCrc.Value());
    }

    unsigned int Count() const { return mEntries.Size(); }

    // For testing: clear all entries
    void Clear() { mEntries.RemoveAll(); }

private:
    PolymorphicRegistry() = default;
    Dia::Core::Containers::DynamicArrayC<PolymorphicEntry, kMaxTypes> mEntries;
};

} // namespace Dia::Reflect
