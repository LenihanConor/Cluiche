#include <DiaAttribute/AttributeSchema.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>

#include <fstream>

namespace Dia::Attribute {

    // -----------------------------------------------------------------------
    // AttributeSchema — construction / destruction
    // -----------------------------------------------------------------------
    AttributeSchema::AttributeSchema()
        : mSchemaName()
        , mAttributes()
        , mBandSchemaVersion(0)
        , mIsValid(false)
    {}

    AttributeSchema::~AttributeSchema()
    {}

    // -----------------------------------------------------------------------
    // Deep copy — DynamicArray's copy-ctor calls At(0) which asserts on empty
    // arrays, so use Reserve+Add to safely handle the zero-attribute case
    // (mirrors DiaEconomy::EconomySchema's defensive copy pattern).
    // -----------------------------------------------------------------------
    AttributeSchema::AttributeSchema(const AttributeSchema& other)
        : mSchemaName(other.mSchemaName)
        , mAttributes()
        , mBandSchemaVersion(other.mBandSchemaVersion)
        , mIsValid(other.mIsValid)
    {
        const unsigned int count = other.mAttributes.Size();
        if (count > 0)
        {
            mAttributes.Reserve(count);
            for (unsigned int i = 0; i < count; ++i)
                mAttributes.Add(other.mAttributes[i]);
        }
    }

    AttributeSchema& AttributeSchema::operator=(const AttributeSchema& other)
    {
        if (this == &other)
            return *this;

        AttributeSchema tmp(other);

        mSchemaName          = tmp.mSchemaName;
        mBandSchemaVersion   = tmp.mBandSchemaVersion;
        mIsValid             = tmp.mIsValid;

        mAttributes.RemoveAll();
        const unsigned int n = tmp.mAttributes.Size();
        if (n > 0)
        {
            if (mAttributes.Capacity() < n)
                mAttributes.Reserve(n);
            for (unsigned int i = 0; i < n; ++i)
                mAttributes.Add(tmp.mAttributes[i]);
        }
        return *this;
    }

    // -----------------------------------------------------------------------
    // Private parsing helpers
    // -----------------------------------------------------------------------
    bool AttributeSchema::ValidateBandRange(const AttributeDefinition& def, Dia::Core::StringCRC band_name, const BandRange& range)
    {
        // AC-6: Reject band range where min > max
        if (range.min > range.max)
        {
            DIA_LOG_WARNING("Attribute",
                "AttributeSchema: attribute '%s' band '%s' has min (%.4f) > max (%.4f) — invalid",
                def.attribute_name.AsChar(), band_name.AsChar(), range.min, range.max);
            return false;
        }

        // AC-7: Reject band range outside attribute's [minimum_value, maximum_value]
        if (range.min < def.minimum_value || range.max > def.maximum_value)
        {
            DIA_LOG_WARNING("Attribute",
                "AttributeSchema: attribute '%s' band '%s' range [%.4f, %.4f] outside attribute bounds [%.4f, %.4f]",
                def.attribute_name.AsChar(), band_name.AsChar(), range.min, range.max,
                def.minimum_value, def.maximum_value);
            return false;
        }

        return true;
    }

    void AttributeSchema::ParseBands(AttributeDefinition& def, const Json::Value& attrJson)
    {
        if (!attrJson.isMember("bands") || !attrJson["bands"].isObject())
            return;

        const Json::Value& bands = attrJson["bands"];
        const auto memberNames = bands.getMemberNames();

        if (memberNames.empty())
            return;

        // Size the HashTable for the number of bands
        def.bands.SetSize(static_cast<unsigned int>(memberNames.size()), static_cast<unsigned int>(memberNames.size()));

        for (const auto& name : memberNames)
        {
            const Json::Value& bandValue = bands[name];
            if (!bandValue.isArray() || bandValue.size() != 2)
            {
                DIA_LOG_WARNING("Attribute",
                    "AttributeSchema: attribute '%s' band '%s' must be a 2-element array [min, max] — skipped",
                    def.attribute_name.AsChar(), name.c_str());
                continue;
            }

            BandRange range;
            range.min = bandValue[0].asFloat();
            range.max = bandValue[1].asFloat();

            Dia::Core::StringCRC band_name(name.c_str());

            // Validate and only add if valid
            if (ValidateBandRange(def, band_name, range))
            {
                def.bands.Add(band_name, range);
            }
        }
    }

    void AttributeSchema::ParseAttributes(AttributeSchema& schema, const Json::Value& root)
    {
        if (!root.isMember("attributes") || !root["attributes"].isArray())
            return;

        const Json::Value& arr = root["attributes"];
        const unsigned int count = static_cast<unsigned int>(arr.size());
        if (count == 0)
            return;

        schema.mAttributes.Reserve(count);

        for (unsigned int i = 0; i < count; ++i)
        {
            const Json::Value& item = arr[i];
            if (!item.isMember("attribute_name") || !item["attribute_name"].isString())
            {
                DIA_LOG_WARNING("Attribute", "AttributeSchema: attribute at index %u missing 'attribute_name' — skipped", i);
                continue;
            }

            AttributeDefinition def;
            def.attribute_name = Dia::Core::StringCRC(item["attribute_name"].asCString());
            def.minimum_value   = item.isMember("minimum_value") ? item["minimum_value"].asFloat() : 0.0f;
            def.maximum_value   = item.isMember("maximum_value") ? item["maximum_value"].asFloat() : 0.0f;
            def.default_value   = item.isMember("default_value") ? item["default_value"].asFloat() : 0.0f;

            if (def.minimum_value > def.default_value || def.default_value > def.maximum_value)
            {
                schema.mIsValid = false;
                DIA_LOG_WARNING("Attribute",
                    "AttributeSchema: attribute '%s' default_value (%.4f) not in [%.4f, %.4f]",
                    def.attribute_name.AsChar(), def.default_value, def.minimum_value, def.maximum_value);
            }

            // Parse bands (Feature 7)
            ParseBands(def, item);

            schema.mAttributes.Add(def);
        }
    }

    // -----------------------------------------------------------------------
    // LoadFromJson
    // -----------------------------------------------------------------------
    AttributeSchema AttributeSchema::LoadFromJson(const char* json_path)
    {
        DIA_ASSERT(json_path != nullptr, "AttributeSchema::LoadFromJson: json_path must not be null");
        if (json_path == nullptr)
            return AttributeSchema{};

        std::ifstream file(json_path);
        if (!file.is_open())
        {
            DIA_LOG_WARNING("Attribute", "AttributeSchema::LoadFromJson: could not open '%s'", json_path);
            return AttributeSchema{};
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors))
        {
            DIA_LOG_WARNING("Attribute", "AttributeSchema::LoadFromJson: JSON parse error in '%s'", json_path);
            return AttributeSchema{};
        }

        return LoadFromJsonValue(root);
    }

    // -----------------------------------------------------------------------
    // LoadFromJsonValue
    // -----------------------------------------------------------------------
    AttributeSchema AttributeSchema::LoadFromJsonValue(const Json::Value& root)
    {
        if (root.isNull() || !root.isObject())
            return AttributeSchema{};

        AttributeSchema schema;
        schema.mIsValid = true;

        if (root.isMember("schema_name") && root["schema_name"].isString())
            schema.mSchemaName = Dia::Core::StringCRC(root["schema_name"].asCString());

        // Parse band_schema_version (Feature 7, AC-8)
        if (root.isMember("band_schema_version") && root["band_schema_version"].isUInt())
            schema.mBandSchemaVersion = root["band_schema_version"].asUInt();

        ParseAttributes(schema, root);

        DIA_LOG_INFO("Attribute", "AttributeSchema loaded: %s (band_schema_version: %u)",
            schema.mSchemaName.AsChar(), schema.mBandSchemaVersion);

        return schema;
    }

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------
    const AttributeDefinition* AttributeSchema::FindAttribute(Dia::Core::StringCRC attribute_name) const
    {
        for (unsigned int i = 0; i < mAttributes.Size(); ++i)
        {
            if (mAttributes[i].attribute_name == attribute_name)
                return &mAttributes[i];
        }
        return nullptr;
    }

    unsigned int AttributeSchema::GetAttributeCount() const
    {
        return mAttributes.Size();
    }

    const AttributeDefinition& AttributeSchema::GetAttributeByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mAttributes.Size(), "AttributeSchema::GetAttributeByIndex: index out of range");
        return mAttributes[index];
    }

    Dia::Core::StringCRC AttributeSchema::GetSchemaName() const
    {
        return mSchemaName;
    }

    bool AttributeSchema::IsValid() const
    {
        return mIsValid;
    }

    // -----------------------------------------------------------------------
    // Band-based initialization API (Feature 7)
    // -----------------------------------------------------------------------
    unsigned int AttributeSchema::GetBandSchemaVersion() const
    {
        return mBandSchemaVersion;
    }

    bool AttributeSchema::HasBands(Dia::Core::StringCRC attribute_name) const
    {
        const AttributeDefinition* def = FindAttribute(attribute_name);
        if (def == nullptr)
            return false;
        return def->bands.Size() > 0;
    }

    float AttributeSchema::ResolveBand(Dia::Core::StringCRC attribute_name, Dia::Core::StringCRC band_name) const
    {
        // AC-4, AC-5: Find attribute, check for bands, resolve band name
        const AttributeDefinition* def = FindAttribute(attribute_name);
        if (def == nullptr)
        {
            DIA_LOG_WARNING("Attribute",
                "AttributeSchema::ResolveBand: attribute '%s' not found in schema — using default 0.0",
                attribute_name.AsChar());
            return 0.0f;
        }

        // AC-5: Attribute has no bands field
        if (def->bands.Size() == 0)
        {
            DIA_LOG_WARNING("Attribute",
                "AttributeSchema::ResolveBand: attribute '%s' has no bands defined — using default_value %.4f",
                def->attribute_name.AsChar(), def->default_value);
            return def->default_value;
        }

        // AC-4: Band name not in schema
        const BandRange* range = def->bands.TryGetItemConst(band_name);
        if (range == nullptr)
        {
            DIA_LOG_WARNING("Attribute",
                "AttributeSchema::ResolveBand: band '%s' not found for attribute '%s' — using default_value %.4f",
                band_name.AsChar(), def->attribute_name.AsChar(), def->default_value);
            return def->default_value;
        }

        // AC-2: Resolve to midpoint
        return (range->min + range->max) * 0.5f;
    }

} // namespace Dia::Attribute
