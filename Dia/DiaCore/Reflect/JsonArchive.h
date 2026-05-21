#pragma once
#include "DiaCore/Reflect/Archive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/Json/external/json/json.h"

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
