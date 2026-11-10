#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <memory>

namespace Dia { namespace EntityTemplateEditor {

    // Max description length incl. terminator. Mirrors ValueInfo::kMaxDescriptionLength (1023)
    // in DiaValueSchema; copied (not included) to keep this module independent of it.
    static constexpr unsigned int kSchemaFieldDescriptionSize = 1024;

    struct SchemaFieldEntry {
        char name[64];
        char kind[32];    // "primitive", "string_id", etc.

        // Optional metadata written by ComponentSchemaJson; empty / has* == false when absent.
        // Text is truncated to the buffer size (always NUL terminated).
        char label[64];
        char description[kSchemaFieldDescriptionSize];
        char unit[32];
        char hint[64];
        double min = 0.0;
        double max = 0.0;
        bool hasMin = false;
        bool hasMax = false;
        // "default" kept as compact JSON text (Json::Value is non-trivial and the
        // containing DynamicArrayC memcpys). hasDefault is false if absent or too long to fit.
        char defaultJson[256];
        bool hasDefault = false;

        SchemaFieldEntry()
        {
            name[0] = kind[0] = label[0] = description[0] = unit[0] = hint[0] = defaultJson[0] = '\0';
        }
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
        SchemaReader();
        ~SchemaReader();
        SchemaReader(const SchemaReader&) = delete;
        SchemaReader& operator=(const SchemaReader&) = delete;

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
        typedef Dia::Core::Containers::DynamicArrayC<SchemaComponentEntry, kMaxComponents> ComponentArray;
        // Heap allocated: field metadata makes the array several MB, too big for the stack.
        std::unique_ptr<ComponentArray> mComponentsPtr;
        ComponentArray& mComponents;
        Json::Value mDefaultValues[kMaxComponents];  // parallel array — safe storage for non-trivial type
        Version mVersion;
        bool    mLoaded = false;
    };

}} // namespace Dia::EntityTemplateEditor
