#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaCore/Containers/HashTables/HashTable.h>

// Forward-declare Json::Value so callers don't need json.h via this header
namespace Json { class Value; }

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // BandRange
    // -----------------------------------------------------------------------
    struct BandRange
    {
        float min;
        float max;
    };

    // -----------------------------------------------------------------------
    // AttributeDefinition
    // -----------------------------------------------------------------------
    struct AttributeDefinition
    {
        Dia::Core::StringCRC attribute_name;
        float                minimum_value;
        float                maximum_value;
        float                default_value;
        Dia::Core::Containers::HashTable<Dia::Core::StringCRC, BandRange> bands;
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

        // Band-based initialization support (Feature 7)
        unsigned int GetBandSchemaVersion() const;
        bool         HasBands(Dia::Core::StringCRC attribute_name) const;

        // Internal API: Resolves a band name to a float value (midpoint of range).
        // Returns the attribute's default_value and logs DIA_LOG_WARNING if:
        // - attribute_name not found in schema
        // - attribute has no bands defined
        // - band_name not found in attribute's bands
        float ResolveBand(Dia::Core::StringCRC attribute_name, Dia::Core::StringCRC band_name) const;

    private:
        static void ParseAttributes(AttributeSchema& schema, const Json::Value& root);
        static void ParseBands(AttributeDefinition& def, const Json::Value& attrJson);
        static bool ValidateBandRange(const AttributeDefinition& def, Dia::Core::StringCRC band_name, const BandRange& range);

        Dia::Core::StringCRC                                      mSchemaName;
        Dia::Core::Containers::DynamicArray<AttributeDefinition>  mAttributes;
        unsigned int                                              mBandSchemaVersion;
        bool                                                      mIsValid;
    };

} // namespace Dia::Attribute
