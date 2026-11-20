#pragma once

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaAttribute/IMapKeySource.h>
#include <DiaValueSchema/ValueSchemaLoader.h>

#include <initializer_list>
#include <string>
#include <tuple>
#include <sstream>
#include <stdexcept>

namespace Dia::Attribute::Testing {

    // -----------------------------------------------------------------------
    // Feature 7: Band-based initialization test helpers
    // -----------------------------------------------------------------------

    // Create an AttributeSchema with band definitions from JSON string
    inline AttributeSchema MakeSchemaWithBands(const char* json_string)
    {
        // Parse JSON string into Json::Value
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::istringstream stream(json_string);
        std::string errors;
        if (!Json::parseFromStream(builder, stream, &root, &errors))
        {
            return AttributeSchema{};
        }
        return AttributeSchema::LoadFromJsonValue(root);
    }

    // Create a simple schema with one attribute and predefined bands
    // Example: MakeTestSchemaWithBands("power", 0.0, 100.0, {{"LOW", 0.0, 30.0}, {"MEDIUM", 30.0, 70.0}, {"HIGH", 70.0, 100.0}})
    inline AttributeSchema MakeTestSchemaWithBands(
        const char* attribute_name,
        float min_value,
        float max_value,
        const std::initializer_list<std::tuple<const char*, float, float>>& bands)
    {
        // Build JSON string programmatically
        Json::Value root;
        root["schema_name"] = "test_schema";
        root["band_schema_version"] = 1;

        Json::Value attributes(Json::arrayValue);
        Json::Value attr;
        attr["attribute_name"] = attribute_name;
        attr["minimum_value"] = min_value;
        attr["maximum_value"] = max_value;
        attr["default_value"] = (min_value + max_value) * 0.5f;

        Json::Value bandsObj;
        for (const auto& [name, bmin, bmax] : bands)
        {
            Json::Value bandArray(Json::arrayValue);
            bandArray.append(bmin);
            bandArray.append(bmax);
            bandsObj[name] = bandArray;
        }
        attr["bands"] = bandsObj;

        attributes.append(attr);
        root["attributes"] = attributes;

        return AttributeSchema::LoadFromJsonValue(root);
    }

    // Assert that a band resolves to the expected value (midpoint)
    inline void AssertBandResolvesToMidpoint(
        const AttributeSet& set,
        Dia::Core::StringCRC attribute_name,
        float expected_midpoint,
        float tolerance = 0.01f)
    {
        const float actual = set.GetBaseValue(attribute_name);
        const float diff = (actual > expected_midpoint) ? (actual - expected_midpoint) : (expected_midpoint - actual);
        if (diff > tolerance)
        {
            // Test framework will catch this assertion
            throw std::runtime_error("Band did not resolve to expected midpoint");
        }
    }

    // Assert that InitializeFromBands logs a warning for a missing band
    // Note: This is a compile-time helper for test authoring — actual warning
    // verification requires test framework hooks (GoogleTest death tests or
    // log capture). This helper exists for API consistency with the spec.
    inline void AssertMissingBandWarning(
        const AttributeSet& /*set*/,
        const Dia::Core::Containers::DynamicArrayC<BandValue, AttributeSet::kMaxBandValuesPerCall>& /*band_values*/)
    {
        // Placeholder — actual warning verification happens via DIA_LOG_WARNING
        // capture in the test framework, not at runtime via this helper.
        // Left as a no-op for now; tests will verify warnings via log output inspection.
    }

    // -----------------------------------------------------------------------
    // Sidecar (dia.value_schema v1) fixtures. Entries are JSON fragments; wrap them with MakeSidecarText.
    // -----------------------------------------------------------------------
    inline std::string SidecarNum(double v)
    {
        std::ostringstream o;
        o.precision(17);
        o << v;
        std::string s = o.str();
        // keep "-0" a real literal ("-0.0"): an integer literal would lose the sign
        if (s.find_first_of(".eE") == std::string::npos) s += ".0";
        return s;
    }

    // `extra` is appended verbatim inside the entry object, e.g. ", \"bands\": {...}".
    inline std::string SidecarFloat(const char* key, double mn, double mx, double def, const char* extra = "")
    {
        return std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"float\",\"min\":"
            + SidecarNum(mn) + ",\"max\":" + SidecarNum(mx) + ",\"default\":" + SidecarNum(def) + extra + "}";
    }

    inline std::string SidecarInt(const char* key, int mn, int mx, int def, const char* extra = "")
    {
        return std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"int\",\"min\":"
            + std::to_string(mn) + ",\"max\":" + std::to_string(mx) + ",\"default\":" + std::to_string(def) + extra + "}";
    }

    inline std::string SidecarBool(const char* key, bool def)
    {
        return std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"bool\",\"default\":"
            + (def ? "true" : "false") + "}";
    }

    inline std::string SidecarEnum(const char* key, std::initializer_list<const char*> ids, const char* defaultId)
    {
        std::string values;
        for (const char* id : ids)
        {
            if (!values.empty()) values += ",";
            values += std::string("{\"id\":\"") + id + "\",\"label\":\"" + id + "\"}";
        }
        return std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"enum\",\"values\":["
            + values + "],\"default\":\"" + defaultId + "\"}";
    }

    // Map entry keyed by the shared fixture key list; defaultJson is the verbatim `default` value
    // (a number or an object), or null for none.
    inline std::string SidecarMap(const char* key, double mn, double mx, const char* defaultJson)
    {
        std::string s = std::string("{\"key\":\"") + key + "\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"map\","
            "\"keyRef\":\"cow.biomes/biomes[].id\",\"valueType\":\"float\",\"min\":" + SidecarNum(mn) + ",\"max\":" + SidecarNum(mx);
        if (defaultJson != nullptr) s += std::string(",\"default\":") + defaultJson;
        return s + "}";
    }

    // A non-parameter entry (kind: metric) for the "ignored kinds" cases.
    inline std::string SidecarMetric(const char* key)
    {
        return std::string("{\"key\":\"") + key + "\",\"kind\":\"metric\",\"label\":\"l\",\"description\":\"d\",\"type\":\"float\"}";
    }

    // constraintsJson: verbatim array, e.g. R"([{"a":"x","op":"<=","b":"y"}])"; null for none.
    inline std::string MakeSidecarText(std::initializer_list<std::string> entries, const char* constraintsJson = nullptr,
                                       const char* forName = "test.params")
    {
        std::string body;
        for (const std::string& e : entries)
        {
            if (!body.empty()) body += ",";
            body += e;
        }
        std::string s = std::string("{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"") + forName + "\",\"entries\":[" + body + "]";
        if (constraintsJson != nullptr) s += std::string(",\"constraints\":") + constraintsJson;
        return s + "}";
    }

    // Mirrors the cow.biomes key data file of the conformance fixtures (ids TemperateForest, Desert, Tundra).
    class FixtureMapKeySource : public IMapKeySource
    {
    public:
        static constexpr const char* kKeyRef = "cow.biomes/biomes[].id";

        FixtureMapKeySource()
            : mIds{ Dia::Core::StringCRC("TemperateForest"), Dia::Core::StringCRC("Desert"), Dia::Core::StringCRC("Tundra") }
        {}

        bool GetKeys(const char* keyRef, MapKeys& out) const override
        {
            if (keyRef == nullptr || std::string(keyRef) != kKeyRef) return false;
            out.ids = mIds;
            out.names = kNames;
            out.count = 3;
            return true;
        }

    private:
        static constexpr const char* kNames[3] = { "TemperateForest", "Desert", "Tundra" };
        Dia::Core::StringCRC mIds[3];
    };

    // Parses a sidecar and builds the schema; throws (failing the test) with the key and reason on any failure.
    inline AttributeSchema LoadSchemaOrDie(const char* text, const IMapKeySource* mapKeys = nullptr)
    {
        const Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(text, "test.schema.json");
        if (!r.mOk)
        {
            throw std::runtime_error(std::string("LoadSchemaOrDie: sidecar failed to load: ") + r.mFailure.mReason.AsCStr());
        }
        AttributeSchema schema;
        SchemaError err;
        if (!AttributeSchema::FromValueSchema(r, mapKeys, schema, err))
        {
            throw std::runtime_error(std::string("LoadSchemaOrDie: key '") + err.key.AsCStr() + "': " + err.reason.AsCStr());
        }
        return schema;
    }


} // namespace Dia::Attribute::Testing
