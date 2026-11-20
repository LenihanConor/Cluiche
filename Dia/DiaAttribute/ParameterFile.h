#pragma once

#include <cstdint>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/IMapKeySource.h>

namespace Dia::Attribute
{
    enum class LoadPolicy : uint8_t { Strict, Lenient };
    enum class JsonShape  : uint8_t { Full, Diff };

    struct ParamError
    {
        Dia::Core::Containers::String128 key;
        Dia::Core::Containers::String128 otherKey;
        Dia::Core::Containers::String256 reason;
    };

    struct ParamWarning
    {
        Dia::Core::Containers::String128 key;
        Dia::Core::Containers::String256 reason;
    };

    struct ParamWarnings
    {
        Dia::Core::Containers::DynamicArrayC<ParamWarning, 64> items;
        uint32_t dropped = 0;   // warnings beyond the 64 stored
    };

    // A parameter file: a strictly loaded sidecar (dia.value_schema) plus the AttributeSet holding its base values.
    // Not copyable or movable (owns an AttributeSet).
    class ParameterFile
    {
    public:
        ParameterFile();
        ~ParameterFile();
        ParameterFile(const ParameterFile&)            = delete;
        ParameterFile& operator=(const ParameterFile&) = delete;
        ParameterFile(ParameterFile&&)                 = delete;
        ParameterFile& operator=(ParameterFile&&)      = delete;

        // Strict. Builds the schema and an AttributeSet at the defaults. On failure err names the first key
        // and the file stays unloaded. Fails (changing nothing) if the file is already loaded.
        bool Load(const char* sidecarText, const char* sidecarName, const IMapKeySource* mapKeys, ParamError& err);

        // Lenient and atomic. Rejects (false, base values untouched) a wrong format/version, a non-object pack,
        // or a pack whose merged values break a constraint. Drops (warning) unknown keys, non-whole ints, unknown
        // enum ids, wrong-typed values; clamps (warning) out-of-range values. A missing or mismatching `basedOn`
        // adds exactly one warning (key `basedOn`) and the pack still applies.
        bool ApplyPack(const Json::Value& pack, ParamWarnings& warn, ParamError& err);

        bool IsLoaded() const { return mLoaded; }

        // Hash of the schema meaning (not the values); see ParameterFile.cpp for the exact byte layout.
        uint64_t SchemaHash() const { return mSchemaHash; }

        AttributeSet&          Values();
        const AttributeSet&    Values() const;
        const AttributeSchema& Schema() const { return mSchema; }

    private:
        struct MapInfo
        {
            Dia::Core::Containers::String128 key;
            uint32_t firstIndex = 0;
            uint32_t count      = 0;
        };

        // The one merge core: fills `candidate` (schema order, pre-filled with the base values) from `obj`.
        // Strict: any fault fails and names the key. Lenient: drops/clamps with warnings; constraints on the
        // candidate still fail. Never touches the AttributeSet.
        bool Merge(const Json::Value& obj, LoadPolicy policy, float* candidate, ParamWarnings* warnings, ParamError& err);

        bool MergeValue(const char* key, uint32_t index, const Json::Value& v, LoadPolicy policy,
                        float* candidate, ParamWarnings* warnings, ParamError& err);

        Dia::Core::Containers::String64 mFor;   // sidecar `for`; the pack `format` must equal it
        AttributeSchema mSchema;
        AttributeSet    mValues;
        Dia::Core::Containers::DynamicArray<MapInfo> mMaps;
        uint64_t        mSchemaHash = 0;
        bool            mLoaded = false;
    };
}
