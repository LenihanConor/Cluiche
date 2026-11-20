#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String256.h>
#include <DiaAttribute/IMapKeySource.h>
#include <cstdint>

// Forward-declare Json::Value so callers don't need json.h via this header
namespace Json { class Value; }

// Forward declarations so dependents of this header do not need DiaValueSchema / json include paths.
namespace Dia::ValueSchema {
    struct LoadResult;
    struct ValueInfo;
    enum class ConstraintOp : uint8_t;
}

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
    // SchemaError: first failure found by AttributeSchema::FromValueSchema / CheckConstraints.
    // -----------------------------------------------------------------------
    struct SchemaError
    {
        Dia::Core::Containers::String128 key;
        Dia::Core::Containers::String256 reason;
    };

    // Storage is always a float; the kind says how to read it.
    enum class AttributeKind : uint8_t { Float, Int, Bool, Enum };

    // A constraint resolved to attribute indices (schema order).
    struct ConstraintIdx
    {
        uint32_t                          a  = 0;
        Dia::ValueSchema::ConstraintOp    op{};
        uint32_t                          b  = 0;
    };

    // -----------------------------------------------------------------------
    // AttributeDefinition
    // -----------------------------------------------------------------------
    struct AttributeDefinition
    {
        Dia::Core::StringCRC attribute_name;
        float                minimum_value = 0.0f;
        float                maximum_value = 0.0f;
        float                default_value = 0.0f;
        Dia::Core::Containers::HashTable<Dia::Core::StringCRC, BandRange> bands;
        AttributeKind        kind = AttributeKind::Float;
        // Enum ids (Enum kind only; kept for ToJson) live in the schema's id pool: this definition's
        // ids are pool[enumIdOffset .. enumIdOffset + enumIdCount), in ordinal order. Use AttributeSchema::GetEnumId.
        uint32_t             enumIdOffset = 0;
        uint32_t             enumIdCount  = 0;
        // Sidecar key (map columns: "<key>.<mapKey>"); diagnostics and ToJson.
        Dia::Core::Containers::String128 fullKey;
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

        // Builds a schema from a parsed sidecar (ValueSchemaLoader::LoadFromText). Only `kind: parameter`
        // entries produce definitions, in sidecar file order; a map entry expands to one definition per key
        // from mapKeys (key-source order), named "<key>.<mapKey>". mapKeys may be null when there is no map entry.
        // On failure out is left unchanged and err names the offending key.
        [[nodiscard]] static bool FromValueSchema(const Dia::ValueSchema::LoadResult& sidecar,
                                                  const IMapKeySource* mapKeys,
                                                  AttributeSchema& out, SchemaError& err);

        // Constraints (resolved to attribute indices). CheckConstraints compares baseValues (schema order,
        // count must equal GetAttributeCount()); on a violation err.key is the first key and err.reason names both.
        // Enum id of attribute `attributeIndex` for `ordinal` (< definition.enumIdCount).
        const Dia::Core::Containers::String64& GetEnumId(unsigned int attributeIndex, unsigned int ordinal) const;

        unsigned int          GetConstraintCount() const;
        const ConstraintIdx&  GetConstraintByIndex(unsigned int index) const;
        bool                  CheckConstraints(const float* baseValues, uint32_t count, SchemaError& err) const;

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
        bool BuildColumn(const Dia::ValueSchema::ValueInfo& e, int valueType, const char* fullKey,
                         bool useNumber, double number, const char* text, SchemaError& err);
        void CopyPools(const AttributeSchema& other);
        static void ParseAttributes(AttributeSchema& schema, const Json::Value& root);
        static void ParseBands(AttributeDefinition& def, const Json::Value& attrJson);
        static bool ValidateBandRange(const AttributeDefinition& def, Dia::Core::StringCRC band_name, const BandRange& range);

        Dia::Core::StringCRC                                      mSchemaName;
        Dia::Core::Containers::DynamicArray<AttributeDefinition>  mAttributes;
        Dia::Core::Containers::DynamicArray<ConstraintIdx>        mConstraints;
        Dia::Core::Containers::DynamicArray<Dia::Core::Containers::String64> mEnumIdPool;
        unsigned int                                              mBandSchemaVersion;
        bool                                                      mIsValid;
    };

} // namespace Dia::Attribute
