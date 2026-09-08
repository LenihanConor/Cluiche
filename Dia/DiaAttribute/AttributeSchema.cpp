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

        mSchemaName = tmp.mSchemaName;
        mIsValid    = tmp.mIsValid;

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
    // Private parsing helper
    // -----------------------------------------------------------------------
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

        ParseAttributes(schema, root);

        DIA_LOG_INFO("Attribute", "AttributeSchema loaded: %s", schema.mSchemaName.AsChar());

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

} // namespace Dia::Attribute
