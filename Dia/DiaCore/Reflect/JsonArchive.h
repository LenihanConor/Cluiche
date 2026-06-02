#pragma once
#include "DiaCore/Reflect/Archive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/Reflect/ContainerSpecializations.h"
#include "DiaCore/Reflect/PolymorphicRegistry.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmicrosoft-exception-spec"
#endif
#include "DiaCore/Json/external/json/json.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <type_traits>
#include <concepts>
#include <cstdint>

namespace Dia::Reflect {

// ---------------------------------------------------------
// Serializable concept
// A type T is Serializable with archive A if a free function
//   serialize(ar, value, version)
// exists for it.
// ---------------------------------------------------------
template<typename T, typename A>
concept Serializable = requires(A& ar, T& v) {
    { serialize(ar, v, 0u) };
};

// =========================================================
//  JsonWriteArchive
// =========================================================
class JsonWriteArchive {
public:
    static constexpr int kMaxDepth = 16;

    JsonWriteArchive() : mDepth(0) {
        mRoot = Json::Value(Json::objectValue);
        mNodePtrs[0] = &mRoot;
        mDepth = 1;
    }

    bool IsReading() const { return false; }
    bool IsWriting() const { return true; }

    Json::Value& GetRoot() { return mRoot; }

    // NamedField
    template<typename T>
    JsonWriteArchive& operator&(NamedField<T> field) {
        Json::Value& node = CurrentNode();
        WriteValue(node[field.nameStr], field.value);
        return *this;
    }

    // OwnedPtrField
    template<typename T>
    JsonWriteArchive& operator&(OwnedPtrField<T> field) {
        Json::Value& node = CurrentNode();
        if (field.ptr == nullptr) {
            node[field.nameStr] = Json::Value::null;
        } else {
            node[field.nameStr] = Json::Value(Json::objectValue);
            PushNode(node[field.nameStr]);
            serialize(*this, *field.ptr, 0u);
            PopNode();
        }
        return *this;
    }

    // PolyOwnedPtrField — polymorphic owning pointer with type tag
    template<typename Base>
    JsonWriteArchive& operator&(PolyOwnedPtrField<Base> field) {
        Json::Value& node = CurrentNode();
        if (field.ptr == nullptr) {
            node[field.nameStr] = Json::Value::null;
        } else {
            node[field.nameStr] = Json::Value(Json::objectValue);
            PushNode(node[field.nameStr]);
            CurrentNode()["_type"] = field.concreteTypeName;
            auto* entry = PolymorphicRegistry::Instance().Find(field.concreteTypeCrc);
            if (entry) {
                entry->writeJson(field.ptr, *this);
            }
            PopNode();
        }
        return *this;
    }

    // RefIdField — serialized as a primitive ID value
    template<typename T>
    JsonWriteArchive& operator&(RefIdField<T> field) {
        Json::Value& node = CurrentNode();
        WriteValue(node[field.nameStr], field.id);
        return *this;
    }

private:
    Json::Value mRoot;
    // Stack of pointers into the JSON tree — top is current node
    Json::Value* mNodePtrs[kMaxDepth];
    int mDepth;

    Json::Value& CurrentNode() {
        return *mNodePtrs[mDepth - 1];
    }

    void PushNode(Json::Value& node) {
        mNodePtrs[mDepth] = &node;
        ++mDepth;
    }

    void PopNode() {
        if (mDepth > 0) --mDepth;
    }

    // Write a single value into a Json::Value slot
    template<typename T>
    void WriteValue(Json::Value& slot, const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            slot = value;
        } else if constexpr (std::is_same_v<T, float>) {
            slot = value;
        } else if constexpr (std::is_same_v<T, double>) {
            slot = value;
        } else if constexpr (std::is_same_v<T, int>) {
            slot = value;
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            slot = value;
        } else if constexpr (std::is_same_v<T, short>) {
            slot = static_cast<int>(value);
        } else if constexpr (std::is_same_v<T, unsigned short>) {
            slot = static_cast<unsigned int>(value);
        } else if constexpr (std::is_same_v<T, char>) {
            slot = static_cast<int>(value);
        } else if constexpr (std::is_same_v<T, unsigned char>) {
            slot = static_cast<unsigned int>(value);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            slot = static_cast<Json::Int64>(value);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            slot = static_cast<Json::UInt64>(value);
        } else if constexpr (std::is_same_v<T, long>) {
            slot = static_cast<Json::Int64>(value);
        } else if constexpr (std::is_same_v<T, unsigned long>) {
            slot = static_cast<Json::UInt64>(value);
        } else if constexpr (std::is_array_v<T> && std::is_same_v<std::remove_extent_t<T>, char>) {
            // char[N] — written as a JSON string (C-string convention)
            slot = value;
        } else if constexpr (std::is_array_v<T>) {
            // C-style static array T[N] — written as a JSON array
            constexpr std::size_t Extent = std::extent_v<T>;
            slot = Json::Value(Json::arrayValue);
            for (std::size_t i = 0; i < Extent; ++i) {
                Json::Value elemSlot;
                WriteValue(elemSlot, value[i]);
                slot.append(elemSlot);
            }
        } else if constexpr (IsDynamicArrayC<T>::value) {
            // DynamicArrayC<E,N> — written as a JSON array (only Size() elements)
            slot = Json::Value(Json::arrayValue);
            for (unsigned int i = 0u; i < value.Size(); ++i) {
                Json::Value elemSlot;
                WriteValue(elemSlot, value.At(i));
                slot.append(elemSlot);
            }
        } else if constexpr (requires(const T& s) { { s.AsCStr() } -> std::same_as<const char*>; }) {
            // Dia String type (String<N> and concrete subtypes) — written as a JSON string
            slot = value.AsCStr();
        } else if constexpr (Serializable<T, JsonWriteArchive>) {
            // Nested serializable struct — push a sub-node
            slot = Json::Value(Json::objectValue);
            PushNode(slot);
            T& mutableVal = const_cast<T&>(value);
            serialize(*this, mutableVal, 0u);
            PopNode();
        }
        // Unknown types: silently ignored (safe default)
    }

};

// =========================================================
//  JsonReadArchive
// =========================================================
class JsonReadArchive {
public:
    static constexpr int kMaxDepth = 16;

    explicit JsonReadArchive(const Json::Value& root)
        : mResult()
        , mDepth(0)
    {
        mNodePtrs[0] = &root;
        mDepth = 1;
    }

    bool IsReading() const { return true; }
    bool IsWriting() const { return false; }

    const SerializeResult& GetResult() const { return mResult; }

    // Access current JSON node — used by types that need raw Json::Value access (e.g. opaque instanceData fields)
    const Json::Value& CurrentNodePublic() const { return CurrentNode(); }

    void AddError(SerializeErrorKind kind, Dia::Core::StringCRC fieldName, const char* msg) {
        mResult.AddError(kind, fieldName, msg);
    }

    // NamedField
    template<typename T>
    JsonReadArchive& operator&(NamedField<T> field) {
        const Json::Value& node = CurrentNode();
        if (node.isMember(field.nameStr)) {
            ReadValue(node[field.nameStr], field.value);
        } else if (field.required) {
            mResult.AddError(
                SerializeErrorKind::RequiredFieldMissing,
                field.name,
                field.nameStr
            );
        }
        // If optional and missing: leave value at its C++ default (SD-REFLECT-004)
        return *this;
    }

    // OwnedPtrField
    template<typename T>
    JsonReadArchive& operator&(OwnedPtrField<T> field) {
        const Json::Value& node = CurrentNode();
        if (node.isMember(field.nameStr)) {
            const Json::Value& sub = node[field.nameStr];
            if (!sub.isNull()) {
                if (field.ptr == nullptr) {
                    field.ptr = new T();
                }
                PushNode(sub);
                serialize(*this, *field.ptr, 0u);
                PopNode();
            }
        }
        return *this;
    }

    // PolyOwnedPtrField — polymorphic owning pointer with type tag
    template<typename Base>
    JsonReadArchive& operator&(PolyOwnedPtrField<Base> field) {
        const Json::Value& node = CurrentNode();
        if (!node.isMember(field.nameStr) || node[field.nameStr].isNull()) {
            return *this;
        }
        const Json::Value& sub = node[field.nameStr];
        if (!sub.isMember("_type")) {
            mResult.AddError(SerializeErrorKind::UnknownPolymorphicType, field.name, "_type key missing");
            return *this;
        }
        const char* typeName = sub["_type"].asCString();
        Dia::Core::StringCRC crc(typeName);
        auto* entry = PolymorphicRegistry::Instance().Find(crc.Value());
        if (!entry) {
            mResult.AddError(SerializeErrorKind::UnknownPolymorphicType, field.name, typeName);
            return *this;
        }
        if (field.ptr == nullptr) {
            field.ptr = static_cast<Base*>(entry->factory());
        }
        PushNode(sub);
        entry->readJson(field.ptr, *this);
        PopNode();
        return *this;
    }

    // RefIdField — read a primitive ID value
    template<typename T>
    JsonReadArchive& operator&(RefIdField<T> field) {
        const Json::Value& node = CurrentNode();
        if (node.isMember(field.nameStr)) {
            ReadValue(node[field.nameStr], field.id);
        }
        return *this;
    }

private:
    SerializeResult mResult;
    const Json::Value* mNodePtrs[kMaxDepth];
    int mDepth;

    const Json::Value& CurrentNode() const {
        return *mNodePtrs[mDepth - 1];
    }

    void PushNode(const Json::Value& node) {
        if (mDepth < kMaxDepth) {
            mNodePtrs[mDepth] = &node;
            ++mDepth;
        }
    }

    void PopNode() {
        if (mDepth > 1) --mDepth;
    }

    // Read a single Json::Value into a typed variable
    template<typename T>
    void ReadValue(const Json::Value& src, T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            value = src.asBool();
        } else if constexpr (std::is_same_v<T, float>) {
            value = src.asFloat();
        } else if constexpr (std::is_same_v<T, double>) {
            value = src.asDouble();
        } else if constexpr (std::is_same_v<T, int>) {
            value = src.asInt();
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            value = src.asUInt();
        } else if constexpr (std::is_same_v<T, short>) {
            value = static_cast<short>(src.asInt());
        } else if constexpr (std::is_same_v<T, unsigned short>) {
            value = static_cast<unsigned short>(src.asUInt());
        } else if constexpr (std::is_same_v<T, char>) {
            value = static_cast<char>(src.asInt());
        } else if constexpr (std::is_same_v<T, unsigned char>) {
            value = static_cast<unsigned char>(src.asUInt());
        } else if constexpr (std::is_same_v<T, int64_t>) {
            value = static_cast<int64_t>(src.asInt64());
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            value = static_cast<uint64_t>(src.asUInt64());
        } else if constexpr (std::is_same_v<T, long>) {
            value = static_cast<long>(src.asInt64());
        } else if constexpr (std::is_same_v<T, unsigned long>) {
            value = static_cast<unsigned long>(src.asUInt64());
        } else if constexpr (std::is_array_v<T> && std::is_same_v<std::remove_extent_t<T>, char>) {
            // char[N] — read from a JSON string (C-string convention)
            if (src.isString()) {
                constexpr std::size_t Extent = std::extent_v<T>;
                const char* str = src.asCString();
                std::size_t len = 0u;
                while (str[len] && len < Extent - 1u) ++len;
                for (std::size_t i = 0u; i < len; ++i) value[i] = str[i];
                value[len] = '\0';
            }
        } else if constexpr (std::is_array_v<T>) {
            // C-style static array T[N] — read from a JSON array
            // Tolerant: fewer elements → remaining keep their defaults;
            //            extra elements → silently truncated.
            constexpr std::size_t Extent = std::extent_v<T>;
            if (src.isArray()) {
                std::size_t count = static_cast<std::size_t>(src.size());
                std::size_t readCount = count < Extent ? count : Extent;
                for (std::size_t i = 0u; i < readCount; ++i) {
                    ReadValue(src[static_cast<int>(i)], value[i]);
                }
            }
        } else if constexpr (IsDynamicArrayC<T>::value) {
            // DynamicArrayC<E,N> — read from a JSON array
            // Tolerant: excess elements beyond capacity are silently dropped.
            if (src.isArray()) {
                value.RemoveAll();
                for (int i = 0; i < static_cast<int>(src.size()) && !value.IsFull(); ++i) {
                    typename DynamicArrayCElem<T>::type elem{};
                    ReadValue(src[i], elem);
                    value.Add(elem);
                }
            }
        } else if constexpr (requires(const T& s) { { s.AsCStr() } -> std::same_as<const char*>; } &&
                             requires(const char* p) { T(p); }) {
            // Dia String type (String<N> and concrete subtypes) — read from JSON string
            if (src.isString()) {
                value = T(src.asCString());
            }
        } else if constexpr (Serializable<T, JsonReadArchive>) {
            // Nested serializable struct — push a sub-node
            if (src.isObject()) {
                PushNode(src);
                serialize(*this, value, 0u);
                PopNode();
            }
        }
        // Unknown types: silently ignored
    }
};

// Satisfy Archive concept
static_assert(Archive<JsonWriteArchive>, "JsonWriteArchive must satisfy Dia::Reflect::Archive");
static_assert(Archive<JsonReadArchive>,  "JsonReadArchive must satisfy Dia::Reflect::Archive");

} // namespace Dia::Reflect
