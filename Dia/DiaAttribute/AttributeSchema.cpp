#include <DiaAttribute/AttributeSchema.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>

#include <DiaValueSchema/ValueSchemaLoader.h>

#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
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

    void AttributeSchema::CopyPools(const AttributeSchema& other)
    {
        const unsigned int cc = other.mConstraints.Size();
        if (cc > 0)
        {
            if (mConstraints.Capacity() < cc) mConstraints.Reserve(cc);
            for (unsigned int i = 0; i < cc; ++i) mConstraints.Add(other.mConstraints[i]);
        }
        const unsigned int ec = other.mEnumIdPool.Size();
        if (ec > 0)
        {
            if (mEnumIdPool.Capacity() < ec) mEnumIdPool.Reserve(ec);
            for (unsigned int i = 0; i < ec; ++i) mEnumIdPool.Add(other.mEnumIdPool[i]);
        }
    }

    // -----------------------------------------------------------------------
    // Deep copy — DynamicArray's copy-ctor calls At(0) which asserts on empty
    // arrays, so use Reserve+Add to safely handle the zero-attribute case
    // (mirrors DiaEconomy::EconomySchema's defensive copy pattern).
    // -----------------------------------------------------------------------
    AttributeSchema::AttributeSchema(const AttributeSchema& other)
        : mSchemaName(other.mSchemaName)
        , mAttributes()
        , mConstraints()
        , mEnumIdPool()
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
        CopyPools(other);
    }

    AttributeSchema& AttributeSchema::operator=(const AttributeSchema& other)
    {
        if (this == &other)
            return *this;

        AttributeSchema tmp(other);

        mSchemaName          = tmp.mSchemaName;
        mBandSchemaVersion   = tmp.mBandSchemaVersion;
        mIsValid             = tmp.mIsValid;

        mConstraints.RemoveAll();
        mEnumIdPool.RemoveAll();
        CopyPools(tmp);
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

        // HashTable already sized in ParseAttributes

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

            // CRITICAL: Initialize bands HashTable BEFORE copying into array
            // Check if bands field exists to determine initialization size
            const bool hasBandsField = item.isMember("bands") && item["bands"].isObject();
            if (hasBandsField)
            {
                const auto bandNames = item["bands"].getMemberNames();
                const unsigned int bandCount = static_cast<unsigned int>(bandNames.size());
                if (bandCount > 0)
                    def.bands.SetSize(bandCount, bandCount);
                else
                    def.bands.SetSize(0, 1);  // Empty bands object
            }
            else
            {
                def.bands.SetSize(0, 1);  // No bands field - initialize to safe empty state
            }

            if (def.minimum_value > def.default_value || def.default_value > def.maximum_value)
            {
                schema.mIsValid = false;
                DIA_LOG_WARNING("Attribute",
                    "AttributeSchema: attribute '%s' default_value (%.4f) not in [%.4f, %.4f]",
                    def.attribute_name.AsChar(), def.default_value, def.minimum_value, def.maximum_value);
            }

            // Add to array (now safe to copy)
            schema.mAttributes.Add(def);

            // Parse bands (Feature 7) - operate on the array element directly
            if (hasBandsField)
                ParseBands(schema.mAttributes[schema.mAttributes.Size() - 1], item);
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
    // FromValueSchema (parameter-files plan tasks 8-9)
    // -----------------------------------------------------------------------
    namespace
    {
        using Dia::ValueSchema::ValueInfo;
        using Dia::ValueSchema::ValueType;
        using Dia::ValueSchema::ValueKind;
        using Dia::ValueSchema::ConstraintOp;
        using Dia::ValueSchema::SetText;

        bool Fail(SchemaError& err, const char* key, const char* fmt, ...)
        {
            char buf[256];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buf, sizeof(buf), fmt, args);
            va_end(args);
            SetText(err.key, key);
            SetText(err.reason, buf);
            return false;
        }

        const char* OpText(ConstraintOp op)
        {
            switch (op)
            {
            case ConstraintOp::Lt: return "<";
            case ConstraintOp::Le: return "<=";
            case ConstraintOp::Gt: return ">";
            case ConstraintOp::Ge: return ">=";
            case ConstraintOp::Eq: return "==";
            default:               return "!=";
            }
        }

        bool Holds(float a, ConstraintOp op, float b)
        {
            switch (op)
            {
            case ConstraintOp::Lt: return a <  b;
            case ConstraintOp::Le: return a <= b;
            case ConstraintOp::Gt: return a >  b;
            case ConstraintOp::Ge: return a >= b;
            case ConstraintOp::Eq: return a == b;
            default:               return a != b;
            }
        }

        bool KindFor(ValueType t, AttributeKind& out)
        {
            switch (t)
            {
            case ValueType::Float: out = AttributeKind::Float; return true;
            case ValueType::Int:   out = AttributeKind::Int;   return true;
            case ValueType::Bool:  out = AttributeKind::Bool;  return true;
            case ValueType::Enum:  out = AttributeKind::Enum;  return true;
            default: return false;
            }
        }

        // Resolves the default of one column to a float, validating kind and range.
        bool ResolveDefault(const AttributeDefinition& def, const ValueInfo& e, bool useNumber, double number, const char* text,
                            const char* key, SchemaError& err, float& out)
        {
            switch (def.kind)
            {
            case AttributeKind::Enum:
                if (useNumber) return Fail(err, key, "default must be an enum id string");
                for (unsigned int i = 0; i < e.mEnumValues.Size(); ++i)
                {
                    if (strcmp(e.mEnumValues[i].mId.AsCStr(), text) == 0) { out = static_cast<float>(i); return true; }
                }
                return Fail(err, key, "default '%s' is not a value of the enum", text);
            case AttributeKind::Bool:
                if (!useNumber) return Fail(err, key, "default must be a boolean");
                if (number != 0.0 && number != 1.0) return Fail(err, key, "bool default must be 0 or 1 (got %g)", number);
                out = static_cast<float>(number);
                return true;
            case AttributeKind::Int:
                if (!useNumber) return Fail(err, key, "default must be a number");
                if (number != std::floor(number)) return Fail(err, key, "int default must be a whole number (got %g)", number);
                break;
            default:
                if (!useNumber) return Fail(err, key, "default must be a number");
                break;
            }
            if (number < static_cast<double>(def.minimum_value) || number > static_cast<double>(def.maximum_value))
                return Fail(err, key, "default %g is outside [%g, %g]", number,
                            static_cast<double>(def.minimum_value), static_cast<double>(def.maximum_value));
            out = static_cast<float>(number);
            return true;
        }
    }

    bool AttributeSchema::BuildColumn(const Dia::ValueSchema::ValueInfo& e, int valueTypeInt, const char* fullKey,
                                      bool useNumber, double number, const char* text, SchemaError& err)
    {
        const ValueType vt = static_cast<ValueType>(valueTypeInt);

        if (strlen(fullKey) > Dia::Core::Containers::String128::Capacity() - 1)
            return Fail(err, fullKey, "key is too long");

        AttributeDefinition def;
        def.attribute_name = Dia::Core::StringCRC(fullKey);
        SetText(def.fullKey, fullKey);
        if (!KindFor(vt, def.kind))
            return Fail(err, fullKey, "unsupported parameter type (use float, int, bool or enum)");

        switch (def.kind)
        {
        case AttributeKind::Bool:
            def.minimum_value = 0.0f;
            def.maximum_value = 1.0f;
            break;
        case AttributeKind::Enum:
            if (e.mEnumValues.Size() == 0) return Fail(err, fullKey, "enum has no values");
            def.minimum_value = 0.0f;
            def.maximum_value = static_cast<float>(e.mEnumValues.Size() - 1);
            break;
        default:
            def.minimum_value = e.mHasMin ? static_cast<float>(e.mMin) : -FLT_MAX;
            def.maximum_value = e.mHasMax ? static_cast<float>(e.mMax) : FLT_MAX;
            if (def.minimum_value > def.maximum_value) return Fail(err, fullKey, "min is greater than max");
            break;
        }

        if (!ResolveDefault(def, e, useNumber, number, text, fullKey, err, def.default_value)) return false;

        const unsigned int bandCount = e.mBands.Size();
        if (bandCount > 0) def.bands.SetSize(bandCount, bandCount); else def.bands.SetSize(0, 1);

        if (def.kind == AttributeKind::Enum)
        {
            def.enumIdOffset = mEnumIdPool.Size();
            def.enumIdCount  = e.mEnumValues.Size();
            for (unsigned int i = 0; i < e.mEnumValues.Size(); ++i) mEnumIdPool.Add(e.mEnumValues[i].mId);
        }

        // Copy into the array first, then populate bands on the stored element (the HashTable must be sized before
        // the copy; populating after matches the old loader).
        mAttributes.Add(def);
        AttributeDefinition& stored = mAttributes[mAttributes.Size() - 1];
        for (unsigned int i = 0; i < bandCount; ++i)
        {
            const Dia::ValueSchema::BandInfo& bi = e.mBands[i];
            if (bi.mLo > bi.mHi)
                return Fail(err, fullKey, "band '%s' has lo > hi", bi.mName.AsCStr());
            if (bi.mLo < static_cast<double>(stored.minimum_value) || bi.mHi > static_cast<double>(stored.maximum_value))
                return Fail(err, fullKey, "band '%s' [%g, %g] lies outside [%g, %g]", bi.mName.AsCStr(), bi.mLo, bi.mHi,
                            static_cast<double>(stored.minimum_value), static_cast<double>(stored.maximum_value));
            BandRange range;
            range.min = static_cast<float>(bi.mLo);
            range.max = static_cast<float>(bi.mHi);
            stored.bands.Add(Dia::Core::StringCRC(bi.mName.AsCStr()), range);
        }
        return true;
    }

    bool AttributeSchema::FromValueSchema(const Dia::ValueSchema::LoadResult& sidecar, const IMapKeySource* mapKeys,
                                          AttributeSchema& out, SchemaError& err)
    {
        SetText(err.key, "");
        SetText(err.reason, "");

        if (!sidecar.mOk)
            return Fail(err, sidecar.mFailure.mEntry.AsCStr(), "sidecar failed to load: %s", sidecar.mFailure.mReason.AsCStr());

        // Pass 1: resolve map keys and count definitions / enum ids so the arrays are sized once
        // (DynamicArray growth memcpy-moves elements, which is unsafe for elements owning heap memory).
        unsigned int defCount = 0;
        unsigned int enumPoolCount = 0;
        for (unsigned int i = 0; i < sidecar.mEntries.Size(); ++i)
        {
            const ValueInfo& e = sidecar.mEntries[i];
            if (e.mKind != ValueKind::Parameter) continue;
            unsigned int columns = 1;
            if (e.mType == ValueType::Map)
            {
                if (mapKeys == nullptr)
                    return Fail(err, e.mKey.AsCStr(), "map entry needs a key source but none was supplied");
                MapKeys keys;
                if (!mapKeys->GetKeys(e.mMapKeyRef.AsCStr(), keys))
                    return Fail(err, e.mKey.AsCStr(), "key source does not know keyRef '%s'", e.mMapKeyRef.AsCStr());
                columns = keys.count;
            }
            defCount += columns;
            if (e.mType == ValueType::Enum || (e.mType == ValueType::Map && e.mMapValueType == ValueType::Enum))
                enumPoolCount += columns * e.mEnumValues.Size();
        }

        AttributeSchema built;
        built.mIsValid = true;
        built.mSchemaName = Dia::Core::StringCRC(sidecar.mFor.AsCStr());
        built.mBandSchemaVersion = 1;   // the loader only accepts sidecar version 1 and does not expose the field
        if (defCount > 0) built.mAttributes.Reserve(defCount);
        if (enumPoolCount > 0) built.mEnumIdPool.Reserve(enumPoolCount);

        // Pass 2: definitions in file order
        for (unsigned int i = 0; i < sidecar.mEntries.Size(); ++i)
        {
            const ValueInfo& e = sidecar.mEntries[i];
            if (e.mKind != ValueKind::Parameter) continue;

            if (e.mType != ValueType::Map)
            {
                if (!e.mHasDefault)
                    return Fail(err, e.mKey.AsCStr(), "parameter has no default");
                const bool useNumber = e.mType != ValueType::Enum;
                if (!built.BuildColumn(e, static_cast<int>(e.mType), e.mKey.AsCStr(), useNumber, e.mDefaultNumber, e.mDefaultString.AsCStr(), err))
                    return false;
                continue;
            }

            MapKeys keys;
            mapKeys->GetKeys(e.mMapKeyRef.AsCStr(), keys);   // validated in pass 1
            if (keys.count > 0 && keys.names == nullptr)
                return Fail(err, e.mKey.AsCStr(), "key source returned no key names");
            if (!e.mHasMapDefault && !e.mHasDefault)
                return Fail(err, e.mKey.AsCStr(), "map parameter has no default");

            const bool useNumber = e.mMapValueType != ValueType::Enum;

            if (e.mHasMapDefault)
            {
                // an extra key (not in the key source) fails, naming "<key>.<mapKey>"
                for (unsigned int d = 0; d < e.mMapDefaults.Size(); ++d)
                {
                    bool known = false;
                    for (uint32_t k = 0; k < keys.count && !known; ++k)
                        known = strcmp(keys.names[k], e.mMapDefaults[d].mMapKey.AsCStr()) == 0;
                    if (!known)
                    {
                        char full[256];
                        snprintf(full, sizeof(full), "%s.%s", e.mKey.AsCStr(), e.mMapDefaults[d].mMapKey.AsCStr());
                        return Fail(err, full, "map default names a key that is not in the key source");
                    }
                }
            }

            for (uint32_t k = 0; k < keys.count; ++k)
            {
                char full[256];
                snprintf(full, sizeof(full), "%s.%s", e.mKey.AsCStr(), keys.names[k]);

                double number = e.mDefaultNumber;
                const char* text = e.mDefaultString.AsCStr();
                if (e.mHasMapDefault)
                {
                    const Dia::ValueSchema::MapDefault* found = nullptr;
                    for (unsigned int d = 0; d < e.mMapDefaults.Size(); ++d)
                    {
                        if (strcmp(e.mMapDefaults[d].mMapKey.AsCStr(), keys.names[k]) == 0) { found = &e.mMapDefaults[d]; break; }
                    }
                    if (found == nullptr)
                        return Fail(err, full, "map default is missing this key");
                    number = found->mNumber;
                    text = found->mString.AsCStr();
                }
                if (!built.BuildColumn(e, static_cast<int>(e.mMapValueType), full, useNumber, number, text, err))
                    return false;
            }
        }

        // Constraints: resolve operand keys to attribute indices
        const unsigned int cCount = sidecar.mConstraints.Size();
        if (cCount > 0) built.mConstraints.Reserve(cCount);
        for (unsigned int c = 0; c < cCount; ++c)
        {
            const Dia::ValueSchema::Constraint& con = sidecar.mConstraints[c];
            const char* names[2] = { con.mA.AsCStr(), con.mB.AsCStr() };
            unsigned int idx[2] = { 0, 0 };
            for (int side = 0; side < 2; ++side)
            {
                bool found = false;
                for (unsigned int a = 0; a < built.mAttributes.Size(); ++a)
                {
                    if (strcmp(built.mAttributes[a].fullKey.AsCStr(), names[side]) == 0) { idx[side] = a; found = true; break; }
                }
                if (!found)
                    return Fail(err, names[side], "constraint names an unknown key (or a map column that does not exist)");
                const AttributeKind k = built.mAttributes[idx[side]].kind;
                if (k == AttributeKind::Bool || k == AttributeKind::Enum)
                    return Fail(err, names[side], "constraint key is not numeric (bool and enum keys cannot be compared)");
            }
            ConstraintIdx ci;
            ci.a = idx[0];
            ci.op = con.mOp;
            ci.b = idx[1];
            built.mConstraints.Add(ci);
        }

        // Constraints must hold on the defaults
        if (built.mConstraints.Size() > 0)
        {
            const unsigned int n = built.mAttributes.Size();
            float* defaults = new float[n];
            for (unsigned int a = 0; a < n; ++a) defaults[a] = built.mAttributes[a].default_value;
            const bool ok = built.CheckConstraints(defaults, n, err);
            delete[] defaults;
            if (!ok) return false;
        }

        out = built;
        DIA_LOG_INFO("Attribute", "AttributeSchema built from sidecar '%s': %u attributes, %u constraints",
            built.mSchemaName.AsChar(), built.mAttributes.Size(), built.mConstraints.Size());
        return true;
    }

    unsigned int AttributeSchema::GetConstraintCount() const { return mConstraints.Size(); }

    const ConstraintIdx& AttributeSchema::GetConstraintByIndex(unsigned int index) const
    {
        DIA_ASSERT(index < mConstraints.Size(), "AttributeSchema::GetConstraintByIndex: index out of range");
        return mConstraints[index];
    }

    const Dia::Core::Containers::String64& AttributeSchema::GetEnumId(unsigned int attributeIndex, unsigned int ordinal) const
    {
        DIA_ASSERT(attributeIndex < mAttributes.Size(), "AttributeSchema::GetEnumId: attribute index out of range");
        const AttributeDefinition& def = mAttributes[attributeIndex];
        DIA_ASSERT(ordinal < def.enumIdCount, "AttributeSchema::GetEnumId: ordinal out of range");
        return mEnumIdPool[def.enumIdOffset + ordinal];
    }

    bool AttributeSchema::CheckConstraints(const float* baseValues, uint32_t count, SchemaError& err) const
    {
        if (mConstraints.Size() == 0) return true;
        if (baseValues == nullptr || count != mAttributes.Size())
            return Fail(err, "", "constraint check needs one value per attribute (got %u, schema has %u)", count, mAttributes.Size());

        for (unsigned int c = 0; c < mConstraints.Size(); ++c)
        {
            const ConstraintIdx& ci = mConstraints[c];
            const float a = baseValues[ci.a];
            const float b = baseValues[ci.b];
            if (!Holds(a, ci.op, b))
            {
                const char* ka = mAttributes[ci.a].fullKey.AsCStr();
                const char* kb = mAttributes[ci.b].fullKey.AsCStr();
                return Fail(err, ka, "constraint violated: %s (%g) %s %s (%g) does not hold",
                            ka, static_cast<double>(a), OpText(ci.op), kb, static_cast<double>(b));
            }
        }
        return true;
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
        if (!def->bands.IsSizeSet() || def->bands.Size() == 0)
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
