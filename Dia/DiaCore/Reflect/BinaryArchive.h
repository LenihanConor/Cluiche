#pragma once
#include "DiaCore/Reflect/Archive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

#include <type_traits>
#include <concepts>
#include <cstdint>
#include <cstring>  // memcpy

namespace Dia::Reflect {

// ---------------------------------------------------------
// Forward-declare the Serializable concept used here.
// (Mirrors the declaration in JsonArchive.h — both archives
//  share the same concept because it is archive-parameterised.)
// ---------------------------------------------------------
template<typename T, typename A>
concept BinarySerializable = requires(A& ar, T& v) {
    { serialize(ar, v, 0u) };
};

// =========================================================
//  BinaryWriteArchive
// =========================================================
class BinaryWriteArchive {
public:
    static constexpr uint32_t kBufferSize = 65536u;

    BinaryWriteArchive() : mCursor(0u) {}

    bool IsReading() const { return false; }
    bool IsWriting() const { return true; }

    // Write the 2-byte version header for the current type blob.
    // Call this before the DIA_SERIALIZE block for the outermost type.
    // Nested serializable types get their version written automatically.
    void WriteVersion(uint16_t version) {
        WriteU16(version);
    }

    // Raw access — valid until the archive is destroyed
    const uint8_t* GetData() const { return mBuffer; }
    uint32_t       GetSize() const { return mCursor; }

    // NamedField — arithmetic or nested serializable
    template<typename T>
    BinaryWriteArchive& operator&(NamedField<T> field) {
        WriteField(field.name.Value(), field.value);
        return *this;
    }

    // OwnedPtrField — embedded inline as nested blob
    template<typename T>
    BinaryWriteArchive& operator&(OwnedPtrField<T> field) {
        const uint32_t crc = field.name.Value();
        if (field.ptr == nullptr) {
            // CRC + size=0
            WriteU32(crc);
            WriteU32(0u);
        } else {
            // Measure nested blob in a sub-archive
            BinaryWriteArchive subAr;
            subAr.WriteVersion(0u);
            serialize(subAr, *field.ptr, 0u);
            WriteU32(crc);
            WriteU32(subAr.GetSize());
            WriteBytes(subAr.GetData(), subAr.GetSize());
        }
        return *this;
    }

    // RefIdField — same wire format as NamedField
    template<typename T>
    BinaryWriteArchive& operator&(RefIdField<T> field) {
        WriteField(field.name.Value(), field.id);
        return *this;
    }

private:
    uint8_t  mBuffer[kBufferSize];
    uint32_t mCursor;

    void WriteBytes(const void* data, uint32_t size) {
        if (mCursor + size <= kBufferSize) {
            memcpy(mBuffer + mCursor, data, size);
            mCursor += size;
        }
        // Overflow: silently truncate (PD-004 — no exceptions)
    }

    void WriteU16(uint16_t v) { WriteBytes(&v, 2u); }
    void WriteU32(uint32_t v) { WriteBytes(&v, 4u); }

    // Write a field entry for a plain arithmetic/trivially-copyable value
    template<typename T>
    void WriteArithmetic(uint32_t crc, const T& value) {
        WriteU32(crc);
        WriteU32(static_cast<uint32_t>(sizeof(T)));
        WriteBytes(&value, static_cast<uint32_t>(sizeof(T)));
    }

    // Dispatch: arithmetic vs nested serializable
    template<typename T>
    void WriteField(uint32_t crc, const T& value) {
        if constexpr (BinarySerializable<T, BinaryWriteArchive>) {
            // Nested serializable: measure in sub-archive, embed inline
            BinaryWriteArchive subAr;
            subAr.WriteVersion(0u);
            T& mutableVal = const_cast<T&>(value);
            serialize(subAr, mutableVal, 0u);
            WriteU32(crc);
            WriteU32(subAr.GetSize());
            WriteBytes(subAr.GetData(), subAr.GetSize());
        } else if constexpr (std::is_arithmetic_v<T>) {
            WriteArithmetic(crc, value);
        }
        // Unknown types: silently ignored (safe default, mirrors JsonWriteArchive)
    }
};

// =========================================================
//  BinaryReadArchive
// =========================================================
class BinaryReadArchive {
public:
    // Construct from a raw byte blob (caller owns the data lifetime)
    BinaryReadArchive(const uint8_t* data, uint32_t size)
        : mData(data)
        , mSize(size)
        , mCursor(0u)
        , mResult()
    {
        BuildFieldIndex();
    }

    bool IsReading() const { return true; }
    bool IsWriting() const { return false; }

    const SerializeResult& GetResult() const { return mResult; }

    // Read the 2-byte version header at the current cursor position.
    // This advances the cursor — call before deserializing fields.
    uint16_t ReadVersion() {
        if (mCursor + 2u <= mSize) {
            uint16_t v = 0u;
            memcpy(&v, mData + mCursor, 2u);
            mCursor += 2u;
            return v;
        }
        return 0u;
    }

    // NamedField — arithmetic or nested serializable
    template<typename T>
    BinaryReadArchive& operator&(NamedField<T> field) {
        const uint32_t crc = field.name.Value();
        const FieldIndex* fi = FindField(crc);
        if (fi != nullptr) {
            ReadField(fi->dataOffset, fi->dataSize, field.value);
        } else if (field.required) {
            mResult.AddError(
                SerializeErrorKind::RequiredFieldMissing,
                field.name,
                field.nameStr
            );
        }
        // If optional and missing: leave at C++ default (SD-REFLECT-004)
        return *this;
    }

    // OwnedPtrField — nested blob
    template<typename T>
    BinaryReadArchive& operator&(OwnedPtrField<T> field) {
        const uint32_t crc = field.name.Value();
        const FieldIndex* fi = FindField(crc);
        if (fi != nullptr && fi->dataSize > 0u) {
            if (field.ptr == nullptr) {
                field.ptr = new T();
            }
            BinaryReadArchive subAr(mData + fi->dataOffset, fi->dataSize);
            subAr.ReadVersion();   // consume the nested version header
            serialize(subAr, *field.ptr, 0u);
        }
        return *this;
    }

    // RefIdField — same as NamedField
    template<typename T>
    BinaryReadArchive& operator&(RefIdField<T> field) {
        const uint32_t crc = field.name.Value();
        const FieldIndex* fi = FindField(crc);
        if (fi != nullptr) {
            ReadField(fi->dataOffset, fi->dataSize, field.id);
        }
        return *this;
    }

private:
    // Sparse record of where each field lives in the blob
    struct FieldIndex {
        uint32_t crc;
        uint32_t dataOffset;  // byte offset from mData[0] to first data byte
        uint32_t dataSize;    // byte count of data (excludes the 8-byte header)
    };

    static constexpr unsigned int kMaxFields = 64u;

    const uint8_t*  mData;
    uint32_t        mSize;
    uint32_t        mCursor;     // only used by ReadVersion()
    SerializeResult mResult;
    Dia::Core::Containers::DynamicArrayC<FieldIndex, kMaxFields> mFieldIndex;

    // Scan the blob after the version header and record all field positions.
    // Called once from the constructor.
    void BuildFieldIndex() {
        // The caller is responsible for calling ReadVersion() to consume the
        // 2-byte version header before deserializing.  During construction we
        // scan from the current cursor position (0), but we need to skip the
        // version header ourselves so that the index covers the field entries.
        uint32_t pos = 2u;  // skip 2-byte version header
        if (pos > mSize) return;

        while (pos + 8u <= mSize) {          // 4-byte CRC + 4-byte size minimum
            uint32_t crc  = 0u;
            uint32_t dsize = 0u;
            memcpy(&crc,   mData + pos,       4u);
            memcpy(&dsize, mData + pos + 4u,  4u);
            pos += 8u;

            if (pos + dsize > mSize) break;  // malformed — stop scanning

            if (!mFieldIndex.IsFull()) {
                FieldIndex fi;
                fi.crc        = crc;
                fi.dataOffset = pos;
                fi.dataSize   = dsize;
                mFieldIndex.Add(fi);
            }

            pos += dsize;
        }
    }

    const FieldIndex* FindField(uint32_t crc) const {
        for (unsigned int i = 0u; i < mFieldIndex.Size(); ++i) {
            if (mFieldIndex.At(i).crc == crc) {
                return &mFieldIndex.At(i);
            }
        }
        return nullptr;
    }

    // Dispatch: arithmetic vs nested serializable
    template<typename T>
    void ReadField(uint32_t offset, uint32_t dataSize, T& value) {
        if constexpr (BinarySerializable<T, BinaryReadArchive>) {
            // Nested: create sub-archive over the field's data bytes
            BinaryReadArchive subAr(mData + offset, dataSize);
            subAr.ReadVersion();   // consume the nested version header
            serialize(subAr, value, 0u);
        } else if constexpr (std::is_arithmetic_v<T>) {
            if (dataSize >= static_cast<uint32_t>(sizeof(T))) {
                memcpy(&value, mData + offset, sizeof(T));
            }
        }
        // Unknown types: silently ignored
    }
};

// Satisfy Archive concept
static_assert(Archive<BinaryWriteArchive>, "BinaryWriteArchive must satisfy Dia::Reflect::Archive");
static_assert(Archive<BinaryReadArchive>,  "BinaryReadArchive must satisfy Dia::Reflect::Archive");

} // namespace Dia::Reflect
