#pragma once

#include <stdint.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Json { class Value; }

namespace Dia::SaveGame {

// Typed read handle passed to ISaveable::Deserialize.
// Backed by a const Json::Value reference; the SaveManager owns the lifetime.
// Read returns false when a key is absent or the value type doesn't match.
class LoadContext {
public:
    static const unsigned int kStackDepth = 16;

    explicit LoadContext(const Json::Value& root);

    bool Read(Dia::Core::StringCRC key, int32_t& out) const;
    bool Read(Dia::Core::StringCRC key, float& out) const;
    bool Read(Dia::Core::StringCRC key, bool& out) const;
    // String variant: fills outBuffer up to bufferSize-1 chars, null-terminates.
    bool Read(Dia::Core::StringCRC key, char* outBuffer, unsigned int bufferSize) const;

    bool BeginObject(Dia::Core::StringCRC key);
    void EndObject();

    // Opens the named array and writes its element count into countOut.
    bool BeginArray(Dia::Core::StringCRC key, uint32_t& countOut);
    void EndArray();

    // Move to the next element inside an open array (call before reading it).
    void SetArrayIndex(uint32_t index);

    const Json::Value& Root() const;

private:
    const Json::Value* mRoot;

    struct StackEntry {
        const Json::Value* node;
        int32_t arrayIndex;  // -1 when not inside an array element
    };

    Dia::Core::Containers::DynamicArrayC<StackEntry, kStackDepth> mStack;

    const Json::Value& Current() const;
};

} // namespace Dia::SaveGame
