#include "DiaEconomy/EconomySchema.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>

#include <fstream>
#include <cstring>

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // EconomySchema — construction / destruction
    // -----------------------------------------------------------------------
    EconomySchema::EconomySchema()
        : mSchemaName()
        , mResources()
        , mIncomeRules()
        , mCostTables()
        , mModifiers()
        , mIsValid(false)
    {}

    EconomySchema::~EconomySchema()
    {}

    // -----------------------------------------------------------------------
    // Deep copy helpers
    // NOTE: DynamicArray<T> uses MemoryCopy internally, which is a shallow
    // bitwise copy.  CostTableDef embeds a DynamicArray<CostEntry>, so we
    // must copy each CostTableDef individually to avoid shallow-copy /
    // double-free issues.
    // -----------------------------------------------------------------------
    EconomySchema::EconomySchema(const EconomySchema& other)
        : mSchemaName(other.mSchemaName)
        , mResources()
        , mIncomeRules()
        , mCostTables()
        , mModifiers()
        , mIsValid(other.mIsValid)
    {
        // DynamicArray copy-ctor calls At(0) which asserts on empty arrays.
        // Use Reserve+Add for all arrays to safely handle the zero-size case.
        const unsigned int resCount = other.mResources.Size();
        if (resCount > 0)
        {
            mResources.Reserve(resCount);
            for (unsigned int i = 0; i < resCount; ++i)
                mResources.Add(other.mResources[i]);
        }

        const unsigned int ruleCount = other.mIncomeRules.Size();
        if (ruleCount > 0)
        {
            mIncomeRules.Reserve(ruleCount);
            for (unsigned int i = 0; i < ruleCount; ++i)
                mIncomeRules.Add(other.mIncomeRules[i]);
        }

        const unsigned int modCount = other.mModifiers.Size();
        if (modCount > 0)
        {
            mModifiers.Reserve(modCount);
            for (unsigned int i = 0; i < modCount; ++i)
                mModifiers.Add(other.mModifiers[i]);
        }

        const unsigned int tableCount = other.mCostTables.Size();
        if (tableCount > 0)
        {
            mCostTables.Reserve(tableCount);
            for (unsigned int i = 0; i < tableCount; ++i)
            {
                const CostTableDef& src = other.mCostTables[i];

                // Add a default entry, then fill it in-place to avoid
                // copy-constructing a CostTableDef that owns an inner
                // DynamicArray pointer.
                mCostTables.AddDefault();
                CostTableDef& dst = mCostTables[i];
                dst.table_name = src.table_name;

                const unsigned int entryCount = src.entries.Size();
                if (entryCount > 0)
                {
                    dst.entries.Reserve(entryCount);
                    for (unsigned int j = 0; j < entryCount; ++j)
                    {
                        dst.entries.Add(src.entries[j]);
                    }
                }
            }
        }
    }

    EconomySchema& EconomySchema::operator=(const EconomySchema& other)
    {
        if (this == &other)
            return *this;

        EconomySchema tmp(other);

        // Swap the guts
        mSchemaName  = tmp.mSchemaName;
        mIsValid     = tmp.mIsValid;

        // Use Reserve+Add to safely handle empty arrays (DynamicArray::operator=
        // dereferences mData even for zero-size, which is UB when mData is null).
        mResources.RemoveAll();
        {
            const unsigned int n = tmp.mResources.Size();
            if (n > 0)
            {
                if (mResources.Capacity() < n) mResources.Reserve(n);
                for (unsigned int i = 0; i < n; ++i) mResources.Add(tmp.mResources[i]);
            }
        }
        mIncomeRules.RemoveAll();
        {
            const unsigned int n = tmp.mIncomeRules.Size();
            if (n > 0)
            {
                if (mIncomeRules.Capacity() < n) mIncomeRules.Reserve(n);
                for (unsigned int i = 0; i < n; ++i) mIncomeRules.Add(tmp.mIncomeRules[i]);
            }
        }
        mModifiers.RemoveAll();
        {
            const unsigned int n = tmp.mModifiers.Size();
            if (n > 0)
            {
                if (mModifiers.Capacity() < n) mModifiers.Reserve(n);
                for (unsigned int i = 0; i < n; ++i) mModifiers.Add(tmp.mModifiers[i]);
            }
        }

        // Cost tables need the same in-place approach as the copy constructor
        mCostTables.RemoveAll();
        const unsigned int tableCount = tmp.mCostTables.Size();
        if (tableCount > 0)
        {
            if (mCostTables.Capacity() < tableCount)
                mCostTables.Reserve(tableCount);

            for (unsigned int i = 0; i < tableCount; ++i)
            {
                CostTableDef& src = tmp.mCostTables[i];

                mCostTables.AddDefault();
                CostTableDef& dst = mCostTables[mCostTables.Size() - 1];
                dst.table_name = src.table_name;

                const unsigned int entryCount = src.entries.Size();
                if (entryCount > 0)
                {
                    dst.entries.Reserve(entryCount);
                    for (unsigned int j = 0; j < entryCount; ++j)
                    {
                        dst.entries.Add(src.entries[j]);
                    }
                }
            }
        }
        return *this;
    }

    // -----------------------------------------------------------------------
    // Private parsing helpers
    // -----------------------------------------------------------------------
    void EconomySchema::ParseResources(EconomySchema& schema, const Json::Value& root)
    {
        if (!root.isMember("resources") || !root["resources"].isArray())
            return;

        const Json::Value& arr = root["resources"];
        const unsigned int count = static_cast<unsigned int>(arr.size());
        if (count == 0)
            return;

        schema.mResources.Reserve(count);

        for (unsigned int i = 0; i < count; ++i)
        {
            const Json::Value& item = arr[i];
            if (!item.isMember("resource_name") || !item["resource_name"].isString())
            {
                DIA_LOG_WARNING("Economy", "EconomySchema: resource at index %u missing 'resource_name' — skipped", i);
                continue;
            }

            ResourceDefinition def;
            def.resource_name  = Dia::Core::StringCRC(item["resource_name"].asCString());
            def.minimum_value  = item.isMember("minimum_value")  ? item["minimum_value"].asFloat()  : 0.0f;
            def.maximum_value  = item.isMember("maximum_value")  ? item["maximum_value"].asFloat()  : 0.0f;
            def.starting_value = item.isMember("starting_value") ? item["starting_value"].asFloat() : 0.0f;
            schema.mResources.Add(def);
        }
    }

    void EconomySchema::ParseIncomeRules(EconomySchema& schema, const Json::Value& root)
    {
        if (!root.isMember("income_rules") || !root["income_rules"].isArray())
            return;

        const Json::Value& arr = root["income_rules"];
        const unsigned int count = static_cast<unsigned int>(arr.size());
        if (count == 0)
            return;

        schema.mIncomeRules.Reserve(count);

        for (unsigned int i = 0; i < count; ++i)
        {
            const Json::Value& item = arr[i];

            IncomeRule rule;
            rule.rule_name         = item.isMember("rule_name")         ? Dia::Core::StringCRC(item["rule_name"].asCString())         : Dia::Core::StringCRC("");
            rule.resource_name     = item.isMember("resource_name")     ? Dia::Core::StringCRC(item["resource_name"].asCString())     : Dia::Core::StringCRC("");
            rule.amount_per_second = item.isMember("amount_per_second") ? item["amount_per_second"].asFloat() : 0.0f;
            schema.mIncomeRules.Add(rule);
        }
    }

    void EconomySchema::ParseCostTables(EconomySchema& schema, const Json::Value& root)
    {
        if (!root.isMember("cost_tables") || !root["cost_tables"].isObject())
            return;

        const Json::Value& tablesObj = root["cost_tables"];
        const Json::Value::Members tableNames = tablesObj.getMemberNames();
        const unsigned int tableCount = static_cast<unsigned int>(tableNames.size());
        if (tableCount == 0)
            return;

        schema.mCostTables.Reserve(tableCount);

        for (unsigned int i = 0; i < tableCount; ++i)
        {
            const std::string& tname = tableNames[i];
            const Json::Value& tableItem = tablesObj[tname];

            schema.mCostTables.AddDefault();
            CostTableDef& tableDef = schema.mCostTables[schema.mCostTables.Size() - 1];
            tableDef.table_name = Dia::Core::StringCRC(tname.c_str());

            // Each key inside the table object is a named entry (e.g. "footsoldier")
            // whose value is an object mapping resource_name -> cost float.
            // Skip "description" which is a string, not a cost entry.
            Json::Value::Members entryNames = tableItem.getMemberNames();
            const unsigned int entryCount = static_cast<unsigned int>(entryNames.size());
            if (entryCount == 0)
                continue;

            tableDef.entries.Reserve(entryCount);

            for (unsigned int j = 0; j < entryCount; ++j)
            {
                const std::string& entryKey = entryNames[j];
                const Json::Value& entryVal = tableItem[entryKey];

                // "description" is metadata, not a cost row
                if (!entryVal.isObject())
                    continue;

                // entryVal is e.g. { "gold": 50.0, "food": 1.0 }
                Json::Value::Members resourceNames = entryVal.getMemberNames();
                for (const std::string& resName : resourceNames)
                {
                    const Json::Value& costVal = entryVal[resName];
                    if (!costVal.isNumeric())
                        continue;

                    // Encode as table_name="unit_costs", resource_name="gold" with
                    // the entry key embedded in a composite key: "entryKey:resName"
                    // so GetCost(table_name, resource_name) can still match.
                    // HOWEVER: spec GetCost(table_name, resource_name) queries by resource name
                    // across all entries in the table — store resource_name + cost directly,
                    // keeping the first occurrence wins (matching spec behavior).
                    CostEntry entry;
                    entry.resource_name = Dia::Core::StringCRC(resName.c_str());
                    entry.cost          = costVal.asFloat();
                    tableDef.entries.Add(entry);
                }
            }
        }
    }

    void EconomySchema::ParseModifiers(EconomySchema& schema, const Json::Value& root)
    {
        if (!root.isMember("modifiers") || !root["modifiers"].isArray())
            return;

        const Json::Value& arr = root["modifiers"];
        const unsigned int count = static_cast<unsigned int>(arr.size());
        if (count == 0)
            return;

        schema.mModifiers.Reserve(count);

        for (unsigned int i = 0; i < count; ++i)
        {
            const Json::Value& item = arr[i];

            ModifierDef mod;
            mod.modifier_name = item.isMember("modifier_name") ? Dia::Core::StringCRC(item["modifier_name"].asCString()) : Dia::Core::StringCRC("");
            mod.resource_name = item.isMember("resource_name") ? Dia::Core::StringCRC(item["resource_name"].asCString()) : Dia::Core::StringCRC("");
            mod.operation     = item.isMember("operation")     ? Dia::Core::StringCRC(item["operation"].asCString())     : Dia::Core::StringCRC("");
            mod.value         = item.isMember("value")         ? item["value"].asFloat() : 0.0f;
            mod.when_condition[0] = '\0';
            if (item.isMember("when_condition") && item["when_condition"].isString())
            {
                strncpy_s(mod.when_condition, sizeof(mod.when_condition),
                          item["when_condition"].asCString(), _TRUNCATE);
            }
            schema.mModifiers.Add(mod);
        }
    }

    // -----------------------------------------------------------------------
    // LoadFromJson
    // -----------------------------------------------------------------------
    EconomySchema EconomySchema::LoadFromJson(const char* json_path)
    {
        DIA_ASSERT(json_path != nullptr, "EconomySchema::LoadFromJson: json_path must not be null");

        std::ifstream file(json_path);
        DIA_ASSERT(file.is_open(), "EconomySchema: could not open JSON file");
        if (!file.is_open())
            return EconomySchema{};

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors))
        {
            DIA_ASSERT(false, "EconomySchema: JSON parse error");
            return EconomySchema{};
        }

        EconomySchema schema;

        if (root.isMember("schema_name") && root["schema_name"].isString())
            schema.mSchemaName = Dia::Core::StringCRC(root["schema_name"].asCString());

        ParseResources  (schema, root);
        ParseIncomeRules(schema, root);
        ParseCostTables (schema, root);
        ParseModifiers  (schema, root);

        schema.mIsValid = true;

        DIA_LOG_INFO("Economy", "EconomySchema loaded: %s", schema.mSchemaName.AsChar());

        return schema;
    }

    // -----------------------------------------------------------------------
    // LoadFromJsonValue
    // -----------------------------------------------------------------------
    EconomySchema EconomySchema::LoadFromJsonValue(const Json::Value& root)
    {
        if (root.isNull() || !root.isObject())
            return EconomySchema{};

        EconomySchema schema;

        if (root.isMember("schema_name") && root["schema_name"].isString())
            schema.mSchemaName = Dia::Core::StringCRC(root["schema_name"].asCString());

        ParseResources  (schema, root);
        ParseIncomeRules(schema, root);
        ParseCostTables (schema, root);
        ParseModifiers  (schema, root);

        schema.mIsValid = true;

        return schema;
    }

    // -----------------------------------------------------------------------
    // Resource queries
    // -----------------------------------------------------------------------
    const ResourceDefinition* EconomySchema::FindResource(Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mResources.Size(); ++i)
        {
            if (mResources[i].resource_name == resource_name)
                return &mResources[i];
        }
        return nullptr;
    }

    unsigned int EconomySchema::GetResourceCount() const
    {
        return mResources.Size();
    }

    const ResourceDefinition& EconomySchema::GetResourceByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mResources.Size(), "EconomySchema::GetResourceByIndex: index out of range");
        return mResources[index];
    }

    // -----------------------------------------------------------------------
    // Cost table queries
    // -----------------------------------------------------------------------
    float EconomySchema::GetCost(Dia::Core::StringCRC table_name,
                                 Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mCostTables.Size(); ++i)
        {
            if (mCostTables[i].table_name == table_name)
            {
                const CostTableDef& table = mCostTables[i];
                for (unsigned int j = 0; j < table.entries.Size(); ++j)
                {
                    if (table.entries[j].resource_name == resource_name)
                        return table.entries[j].cost;
                }
                return 0.0f; // table found, resource not in it
            }
        }
        return 0.0f; // table not found
    }

    // -----------------------------------------------------------------------
    // Income rule queries
    // -----------------------------------------------------------------------
    unsigned int EconomySchema::GetIncomeRuleCount() const
    {
        return mIncomeRules.Size();
    }

    const IncomeRule& EconomySchema::GetIncomeRuleByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mIncomeRules.Size(), "EconomySchema::GetIncomeRuleByIndex: index out of range");
        return mIncomeRules[index];
    }

    // -----------------------------------------------------------------------
    // Modifier queries
    // -----------------------------------------------------------------------
    unsigned int EconomySchema::GetModifierCount() const
    {
        return mModifiers.Size();
    }

    const ModifierDef& EconomySchema::GetModifierByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mModifiers.Size(), "EconomySchema::GetModifierByIndex: index out of range");
        return mModifiers[index];
    }

    // -----------------------------------------------------------------------
    // Metadata
    // -----------------------------------------------------------------------
    Dia::Core::StringCRC EconomySchema::GetSchemaName() const
    {
        return mSchemaName;
    }

    bool EconomySchema::IsValid() const
    {
        return mIsValid;
    }

    // -----------------------------------------------------------------------
    // EconomySchemaValidator
    // -----------------------------------------------------------------------
    SchemaValidationResult EconomySchemaValidator::Validate(const EconomySchema& schema)
    {
        SchemaValidationResult result;
        result.isValid        = true;
        result.errorMessage[0] = '\0';

        if (!schema.IsValid())
        {
            result.isValid = false;
            strncpy_s(result.errorMessage, sizeof(result.errorMessage),
                      "Schema is not valid (failed to load or was never loaded)", _TRUNCATE);
            return result;
        }

        if (schema.GetResourceCount() == 0)
        {
            DIA_LOG_WARNING("Economy", "EconomySchemaValidator: schema '%s' defines no resources",
                            schema.GetSchemaName().AsChar());
        }

        // Validate each resource definition
        for (unsigned int i = 0; i < schema.GetResourceCount(); ++i)
        {
            const ResourceDefinition& res = schema.GetResourceByIndex(i);

            if (res.minimum_value > res.maximum_value)
            {
                result.isValid = false;
                strncpy_s(result.errorMessage, sizeof(result.errorMessage),
                          "Resource minimum_value exceeds maximum_value", _TRUNCATE);
                DIA_LOG_WARNING("Economy",
                    "EconomySchemaValidator: resource '%s' minimum_value (%.2f) > maximum_value (%.2f)",
                    res.resource_name.AsChar(), res.minimum_value, res.maximum_value);
                return result;
            }

            if (res.starting_value < res.minimum_value || res.starting_value > res.maximum_value)
            {
                result.isValid = false;
                strncpy_s(result.errorMessage, sizeof(result.errorMessage),
                          "Resource starting_value is outside [minimum_value, maximum_value]", _TRUNCATE);
                DIA_LOG_WARNING("Economy",
                    "EconomySchemaValidator: resource '%s' starting_value (%.2f) not in [%.2f, %.2f]",
                    res.resource_name.AsChar(), res.starting_value, res.minimum_value, res.maximum_value);
                return result;
            }
        }

        // Validate income rules reference known resources
        for (unsigned int i = 0; i < schema.GetIncomeRuleCount(); ++i)
        {
            const IncomeRule& rule = schema.GetIncomeRuleByIndex(i);
            if (schema.FindResource(rule.resource_name) == nullptr)
            {
                DIA_LOG_WARNING("Economy",
                    "EconomySchemaValidator: income_rule '%s' references unknown resource '%s'",
                    rule.rule_name.AsChar(), rule.resource_name.AsChar());
            }
        }

        return result;
    }

}} // namespace Dia::Economy
