#pragma once

#include <stdint.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::SaveGame {

// Typed read/write handle passed to ISaveable::Deserialize and migration callbacks.
// Owns a mutable copy of the JSON node so migrations can transform data in-place:
//   ctx.Read(key, old); ctx.Write(key, transformed);
// Deserialize sees the post-migration state when it reads the same keys.
class LoadContext {
public:
    static const unsigned int kStackDepth = 16;

    explicit LoadContext(const Json::Value& root);

    // --- Read ---
    bool Read(Dia::Core::StringCRC key, int32_t& out) const;
    bool Read(Dia::Core::StringCRC key, int64_t& out) const;
    bool Read(Dia::Core::StringCRC key, float& out) const;
    bool Read(Dia::Core::StringCRC key, bool& out) const;
    bool Read(Dia::Core::StringCRC key, char* outBuffer, unsigned int bufferSize) const;

    // --- Write (for use in migration callbacks) ---
    void Write(Dia::Core::StringCRC key, int32_t value);
    void Write(Dia::Core::StringCRC key, int64_t value);
    void Write(Dia::Core::StringCRC key, float value);
    void Write(Dia::Core::StringCRC key, bool value);
    void Write(Dia::Core::StringCRC key, const char* value);

    // --- Navigation ---
    bool BeginObject(Dia::Core::StringCRC key);
    void EndObject();
    bool BeginArray(Dia::Core::StringCRC key, uint32_t& countOut);
    void EndArray();
    void SetArrayIndex(uint32_t index);

    const Json::Value& Root() const;
    const Json::Value& CurrentNode() const;

private:
    Json::Value mData; // owned mutable copy — migrations write here, Deserialize reads here

    struct StackEntry {
        Json::Value* node;
        int32_t      arrayIndex; // -1 when not inside an array element
    };

    Dia::Core::Containers::DynamicArrayC<StackEntry, kStackDepth> mStack;

    Json::Value&       MutableCurrent();
    const Json::Value& Current() const;
};

} // namespace Dia::SaveGame
