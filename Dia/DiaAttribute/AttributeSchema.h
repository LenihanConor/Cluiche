#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>

// Forward-declare Json::Value so callers don't need json.h via this header
namespace Json { class Value; }

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // AttributeDefinition
    // -----------------------------------------------------------------------
    struct AttributeDefinition
    {
        Dia::Core::StringCRC attribute_name;
        float                minimum_value;
        float                maximum_value;
        float                default_value;
    };

    // -----------------------------------------------------------------------
    // AttributeSchema
    // -----------------------------------------------------------------------
    class AttributeSchema
    {
    public:
        AttributeSchema();
        ~AttributeSchema();

        // Explicit copy construction / assignment so the internal DynamicArray is
        // deep-copied rather than shallow-copied (mirrors DiaEconomy::EconomySchema).
        AttributeSchema(const AttributeSchema& other);
        AttributeSchema& operator=(const AttributeSchema& other);

        // Load from a JSON file on disk. Returns an invalid schema if the file
        // cannot be opened or parsed.
        [[nodiscard]] static AttributeSchema LoadFromJson(const char* json_path);

        // Load from an already-parsed Json::Value (for tests and in-memory pipelines).
        // Returns an invalid schema if the value is null or not an object.
        [[nodiscard]] static AttributeSchema LoadFromJsonValue(const Json::Value& root);

        // Returns nullptr if no attribute with this name exists in the schema.
        const AttributeDefinition* FindAttribute(Dia::Core::StringCRC attribute_name) const;
        unsigned int                GetAttributeCount() const;
        const AttributeDefinition&  GetAttributeByIndex(unsigned int index) const;

        Dia::Core::StringCRC GetSchemaName() const;
        bool                 IsValid() const;

    private:
        static void ParseAttributes(AttributeSchema& schema, const Json::Value& root);

        Dia::Core::StringCRC                                      mSchemaName;
        Dia::Core::Containers::DynamicArray<AttributeDefinition>  mAttributes;
        bool                                                      mIsValid;
    };

} // namespace Dia::Attribute
