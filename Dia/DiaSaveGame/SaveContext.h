#pragma once

#include <stdint.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

// Forward-declare Json::Value to avoid pulling jsoncpp into every consumer
namespace Json { class Value; }

namespace Dia::SaveGame {

// Typed write handle passed to ISaveable::Serialize.
// Backed by jsoncpp; format is always JSON (SaveFormat::Binary is not yet implemented).
// Key ordering in the output matches Write/BeginObject call order.
class SaveContext {
public:
    static const unsigned int kStackDepth   = 16;
    static const unsigned int kBufferSize   = 65536;

    SaveContext();
    ~SaveContext();

    void Write(Dia::Core::StringCRC key, int32_t value);
    void Write(Dia::Core::StringCRC key, float value);
    void Write(Dia::Core::StringCRC key, bool value);
    void Write(Dia::Core::StringCRC key, const char* value);

    void BeginObject(Dia::Core::StringCRC key);
    void EndObject();

    void BeginArray(Dia::Core::StringCRC key);
    void EndArray();

    // Serialise the accumulated tree to a null-terminated JSON string.
    // Returns false if the buffer is too small.
    bool Flush(char* outBuffer, unsigned int bufferSize) const;

    // Direct access to the root and current node (used by SaveManifest to append array elements).
    Json::Value& Root();
    const Json::Value& Root() const;
    Json::Value& CurrentNode();

private:
    Json::Value* mRoot;

    struct StackEntry {
        Json::Value* node;
        Dia::Core::StringCRC key;   // key this node was opened under (empty for root)
        bool isArray;
    };

    Dia::Core::Containers::DynamicArrayC<StackEntry, kStackDepth> mStack;

    Json::Value& Current();
};

} // namespace Dia::SaveGame
