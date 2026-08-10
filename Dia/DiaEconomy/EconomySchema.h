#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaCore/Containers/HashTables/HashTable.h>

// Forward-declare Json::Value so callers don't need json.h via this header
namespace Json { class Value; }

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // ResourceDefinition
    // -----------------------------------------------------------------------
    struct ResourceDefinition
    {
        Dia::Core::StringCRC resource_name;
        float                minimum_value;
        float                maximum_value;
        float                starting_value;
    };

    // -----------------------------------------------------------------------
    // IncomeRule
    // -----------------------------------------------------------------------
    struct IncomeRule
    {
        Dia::Core::StringCRC rule_name;
        Dia::Core::StringCRC resource_name;
        float                amount_per_second;
        // 'when' condition deferred to Task 6 (conditional rules)
    };

    // -----------------------------------------------------------------------
    // ModifierDef  (parsed now; applied in Task 6)
    // -----------------------------------------------------------------------
    struct ModifierDef
    {
        Dia::Core::StringCRC modifier_name;
        Dia::Core::StringCRC resource_name;
        Dia::Core::StringCRC operation;       // "multiply_income", "multiply_cap", "flat_income"
        float                value;
        char                 when_condition[128]; // empty = always-on; non-empty = conditional
    };

    // -----------------------------------------------------------------------
    // Cost table storage
    // -----------------------------------------------------------------------
    struct CostEntry
    {
        Dia::Core::StringCRC resource_name;
        float                cost;
    };

    struct CostTableDef
    {
        Dia::Core::StringCRC                                   table_name;
        Dia::Core::Containers::DynamicArray<CostEntry>         entries;
    };

    // -----------------------------------------------------------------------
    // EconomySchema
    // -----------------------------------------------------------------------
    class EconomySchema
    {
    public:
        EconomySchema();
        ~EconomySchema();

        // Explicit copy construction / assignment so nested DynamicArrays are
        // deep-copied rather than shallow-copied.
        EconomySchema(const EconomySchema& other);
        EconomySchema& operator=(const EconomySchema& other);

        // Load from a JSON file on disk.  Returns an empty (invalid) schema
        // if the file cannot be opened or parsed.
        [[nodiscard]] static EconomySchema LoadFromJson(const char* json_path);

        // Load from an already-parsed Json::Value (for tests and in-memory pipelines).
        // Returns an empty (invalid) schema if the value is null or cannot be parsed.
        [[nodiscard]] static EconomySchema LoadFromJsonValue(const Json::Value& root);

        // --- resource queries ---
        const ResourceDefinition* FindResource(Dia::Core::StringCRC resource_name) const;
        unsigned int              GetResourceCount() const;
        const ResourceDefinition& GetResourceByIndex(unsigned int index) const;

        // --- cost table queries ---
        // Returns 0.0f if the table or resource is not found.
        float GetCost(Dia::Core::StringCRC table_name,
                      Dia::Core::StringCRC resource_name) const;
        unsigned int        GetCostTableCount() const;
        const CostTableDef& GetCostTableByIndex(unsigned int index) const;

        // --- income rule queries ---
        unsigned int       GetIncomeRuleCount() const;
        const IncomeRule&  GetIncomeRuleByIndex(unsigned int index) const;

        // --- modifier queries ---
        unsigned int        GetModifierCount() const;
        const ModifierDef&  GetModifierByIndex(unsigned int index) const;

        // --- metadata ---
        Dia::Core::StringCRC GetSchemaName() const;
        bool                 IsValid() const;

    private:
        // Allow LoadFromJson to populate private members
        static void ParseResources  (EconomySchema& schema, const Json::Value& root);
        static void ParseIncomeRules(EconomySchema& schema, const Json::Value& root);
        static void ParseCostTables (EconomySchema& schema, const Json::Value& root);
        static void ParseModifiers  (EconomySchema& schema, const Json::Value& root);

        Dia::Core::StringCRC                                        mSchemaName;
        Dia::Core::Containers::DynamicArray<ResourceDefinition>     mResources;
        Dia::Core::Containers::DynamicArray<IncomeRule>             mIncomeRules;
        Dia::Core::Containers::DynamicArray<CostTableDef>           mCostTables;
        Dia::Core::Containers::DynamicArray<ModifierDef>            mModifiers;
        bool                                                        mIsValid;
    };

    // -----------------------------------------------------------------------
    // EconomySchemaValidator
    // -----------------------------------------------------------------------
    struct SchemaValidationResult
    {
        bool isValid;
        char errorMessage[256];
    };

    class EconomySchemaValidator
    {
    public:
        [[nodiscard]] static SchemaValidationResult Validate(const EconomySchema& schema);
    };

}} // namespace Dia::Economy
