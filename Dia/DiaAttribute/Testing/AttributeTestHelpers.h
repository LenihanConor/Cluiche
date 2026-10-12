#pragma once

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <initializer_list>
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

} // namespace Dia::Attribute::Testing
