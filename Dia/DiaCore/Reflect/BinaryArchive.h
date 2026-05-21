#pragma once
#include "DiaCore/Reflect/Archive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/Reflect/ContainerSpecializations.h"
#include "DiaCore/Reflect/PolymorphicRegistry.h"
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

    // NamedField — arithmetic, array, DynamicArrayC, or nested serializable
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

    // PolyOwnedPtrField — polymorphic owning pointer with type CRC tag
    // Binary format: [4-byte field CRC][4-byte total size][4-byte concrete type CRC][version(2) + fields]
    template<typename Base>
    BinaryWriteArchive& operator&(PolyOwnedPtrField<Base> field) {
        const uint32_t crc = field.name.Value();
        if (field.ptr == nullptr) {
            WriteU32(crc);
            WriteU32(0u);
        } else {
            auto* entry = PolymorphicRegistry::Instance().Find(field.concreteTypeCrc);
            if (entry) {
                BinaryWriteArchive subAr;
                // Write the concrete type CRC as the first 4 bytes of the data payload
                subAr.WriteU32(field.concreteTypeCrc);
                // Then version + fields via the registered writeBinary
                subAr.WriteVersion(0u);
                entry->writeBinary(field.ptr, subAr);
                WriteU32(crc);
                WriteU32(subAr.GetSize());
                WriteBytes(subAr.GetData(), subAr.GetSize());
            } else {
                // Unknown type — write as null (size=0)
                WriteU32(crc);
                WriteU32(0u);
            }
        }
        return *this;
    }

    // RefIdField — same wire format as NamedField
    template<typename T>
    BinaryWriteArchive& operator&(RefIdField<T> field) {
        WriteField(field.name.Value(), field.id);
        return *this;
    }

    // WriteBytes is public so that WriteRawValue helpers can call it on
    // a sub-archive instance. (Private WriteBytes would require friendship.)
    void WriteBytes(const void* data, uint32_t size) {
        if (mCursor + size <= kBufferSize) {
            memcpy(mBuffer + mCursor, data, size);
            mCursor += size;
        }
        // Overflow: silently truncate (PD-004 — no exceptions)
    }

    void WriteU32(uint32_t v) { WriteBytes(&v, 4u); }

private:
    uint8_t  mBuffer[kBufferSize];
    uint32_t mCursor;

    void WriteU16(uint16_t v) { WriteBytes(&v, 2u); }

    // Write a field entry for a plain arithmetic/trivially-copyable value
    template<typename T>
    void WriteArithmetic(uint32_t crc, const T& value) {
        WriteU32(crc);
        WriteU32(static_cast<uint32_t>(sizeof(T)));
        WriteBytes(&value, static_cast<uint32_t>(sizeof(T)));
    }

    // Write a raw element value into subAr (no CRC/size header).
    // Used for packing array elements contiguously.
    //   - arithmetic T: raw sizeof(T) bytes
    //   - serializable T: [4-byte blob-size][version(2)][fields...]
    //     The 4-byte size prefix lets the reader skip elements of unknown size.
    template<typename T>
    void WriteRawValue(BinaryWriteArchive& subAr, const T& value) {
        if constexpr (std::is_arithmetic_v<T>) {
            subAr.WriteBytes(&value, static_cast<uint32_t>(sizeof(T)));
        } else if constexpr (BinarySerializable<T, BinaryWriteArchive>) {
            BinaryWriteArchive elemAr;
            elemAr.WriteVersion(0u);
            T& mutableVal = const_cast<T&>(value);
            serialize(elemAr, mutableVal, 0u);
            uint32_t elemSize = elemAr.GetSize();
            subAr.WriteU32(elemSize);
            subAr.WriteBytes(elemAr.GetData(), elemSize);
        }
        // Unknown element types: silently ignored
    }

    // Dispatch: array, DynamicArrayC, nested serializable, or arithmetic
    template<typename T>
    void WriteField(uint32_t crc, const T& value) {
        if constexpr (std::is_array_v<T>) {
            // C-style static array T[N]:
            // data payload = [4-byte count][raw element bytes...]
            constexpr uint32_t N = static_cast<uint32_t>(std::extent_v<T>);
            BinaryWriteArchive subAr;
            subAr.WriteU32(N);
            for (uint32_t i = 0u; i < N; ++i) {
                WriteRawValue(subAr, value[i]);
            }
            WriteU32(crc);
            WriteU32(subAr.GetSize());
            WriteBytes(subAr.GetData(), subAr.GetSize());
        } else if constexpr (IsDynamicArrayC<T>::value) {
            // DynamicArrayC<E,N>:
            // data payload = [4-byte count][raw element bytes... (count elements)]
            uint32_t count = value.Size();
            BinaryWriteArchive subAr;
            subAr.WriteU32(count);
            for (uint32_t i = 0u; i < count; ++i) {
                WriteRawValue(subAr, value.At(i));
            }
            WriteU32(crc);
            WriteU32(subAr.GetSize());
            WriteBytes(subAr.GetData(), subAr.GetSize());
        } else if constexpr (BinarySerializable<T, BinaryWriteArchive>) {
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

    // NamedField — arithmetic, array, DynamicArrayC, or nested serializable
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

    // PolyOwnedPtrField — polymorphic owning pointer with type CRC tag
    // Binary format: [4-byte field CRC][4-byte total size][4-byte concrete type CRC][version(2) + fields]
    template<typename Base>
    BinaryReadArchive& operator&(PolyOwnedPtrField<Base> field) {
        const uint32_t crc = field.name.Value();
        const FieldIndex* fi = FindField(crc);
        if (fi == nullptr || fi->dataSize == 0u) {
            return *this;
        }
        // First 4 bytes of data are the concrete type CRC
        uint32_t offset = fi->dataOffset;
        uint32_t dataEnd = offset + fi->dataSize;
        if (offset + 4u > dataEnd) return *this;

        uint32_t concreteTypeCrc = 0u;
        memcpy(&concreteTypeCrc, mData + offset, 4u);
        offset += 4u;

        auto* entry = PolymorphicRegistry::Instance().Find(concreteTypeCrc);
        if (!entry) {
            mResult.AddError(SerializeErrorKind::UnknownPolymorphicType, field.name, "unknown polymorphic type CRC");
            return *this;
        }
        if (field.ptr == nullptr) {
            field.ptr = static_cast<Base*>(entry->factory());
        }
        // Remaining bytes are [version(2)][fields...]
        uint32_t remainingSize = dataEnd - offset;
        BinaryReadArchive subAr(mData + offset, remainingSize);
        subAr.ReadVersion();
        entry->readBinary(field.ptr, subAr);
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

    // Read a raw element from [data+offset .. data+dataEnd).
    // Mirrors BinaryWriteArchive::WriteRawValue layout.
    // Returns the new offset (i.e. past the bytes consumed).
    template<typename T>
    static uint32_t ReadRawValue(const uint8_t* data, uint32_t offset,
                                 uint32_t dataEnd, T& value) {
        if constexpr (std::is_arithmetic_v<T>) {
            if (offset + static_cast<uint32_t>(sizeof(T)) <= dataEnd) {
                memcpy(&value, data + offset, sizeof(T));
                return offset + static_cast<uint32_t>(sizeof(T));
            }
            return dataEnd; // exhausted
        } else if constexpr (BinarySerializable<T, BinaryReadArchive>) {
            // Read the 4-byte blob-size prefix written by WriteRawValue
            if (offset + 4u > dataEnd) return dataEnd;
            uint32_t elemSize = 0u;
            memcpy(&elemSize, data + offset, 4u);
            offset += 4u;
            if (offset + elemSize > dataEnd) return dataEnd;
            BinaryReadArchive elemAr(data + offset, elemSize);
            elemAr.ReadVersion();
            serialize(elemAr, value, 0u);
            return offset + elemSize;
        }
        return dataEnd; // unknown — skip remaining
    }

    // Dispatch: array, DynamicArrayC, nested serializable, or arithmetic
    template<typename T>
    void ReadField(uint32_t offset, uint32_t dataSize, T& value) {
        if constexpr (std::is_array_v<T>) {
            // C-style static array T[N]:
            // payload = [4-byte written-count][raw element bytes...]
            // Tolerant: fewer written elements → remaining keep defaults;
            //           more written elements than N → silently truncated.
            constexpr uint32_t N = static_cast<uint32_t>(std::extent_v<T>);
            uint32_t dataEnd = offset + dataSize;
            if (offset + 4u > dataEnd) return;
            uint32_t writtenCount = 0u;
            memcpy(&writtenCount, mData + offset, 4u);
            offset += 4u;
            uint32_t readCount = writtenCount < N ? writtenCount : N;
            for (uint32_t i = 0u; i < readCount; ++i) {
                offset = ReadRawValue(mData, offset, dataEnd, value[i]);
            }
        } else if constexpr (IsDynamicArrayC<T>::value) {
            // DynamicArrayC<E,N>:
            // payload = [4-byte written-count][raw element bytes...]
            // Tolerant: excess elements beyond capacity silently dropped.
            uint32_t dataEnd = offset + dataSize;
            if (offset + 4u > dataEnd) return;
            uint32_t writtenCount = 0u;
            memcpy(&writtenCount, mData + offset, 4u);
            offset += 4u;
            value.RemoveAll();
            for (uint32_t i = 0u; i < writtenCount && !value.IsFull(); ++i) {
                typename DynamicArrayCElem<T>::type elem{};
                offset = ReadRawValue(mData, offset, dataEnd, elem);
                value.Add(elem);
            }
        } else if constexpr (BinarySerializable<T, BinaryReadArchive>) {
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
