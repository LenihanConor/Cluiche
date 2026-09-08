#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace EntityTemplateEditor {

    struct SchemaFieldEntry {
        char name[64];
        char kind[32];    // "primitive", "string_id", etc.
    };

    struct SchemaComponentEntry {
        Dia::Core::StringCRC typeId;
        char debugName[128];
        char description[256];
        Dia::Core::Containers::DynamicArrayC<SchemaFieldEntry, 32> fields;
        // defaultValues intentionally NOT here — Json::Value is non-trivial and
        // DynamicArrayC uses memcpy internally, which corrupts it. Stored in
        // SchemaReader::mDefaultValues[] at the matching index.
    };

    class SchemaReader {
    public:
        static constexpr unsigned int kMaxComponents = 64;

        // Load from file. Silently produces empty reader if file missing or malformed.
        void LoadFromFile(const char* schemaPath);

        void Clear();
        bool IsLoaded() const;

        unsigned int GetComponentCount() const;
        const SchemaComponentEntry& GetComponent(unsigned int i) const;

        // Returns the default_values Json::Value for component at index i.
        // Returns Json::Value(Json::nullValue) if not loaded or no default_values key.
        const Json::Value& GetDefaultValues(unsigned int i) const;

        // Returns the loaded version, or {0,0} if not loaded.
        struct Version { int major = 0; int minor = 0; };
        Version GetVersion() const;

    private:
        Dia::Core::Containers::DynamicArrayC<SchemaComponentEntry, kMaxComponents> mComponents;
        Json::Value mDefaultValues[kMaxComponents];  // parallel array — safe storage for non-trivial type
        Version mVersion;
        bool    mLoaded = false;
    };

}} // namespace Dia::EntityTemplateEditor
