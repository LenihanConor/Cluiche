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

        // Strict and atomic: sets one parameter (a scalar key or a map column `<key>.<mapKey>`). Unknown key,
        // out-of-range value, wrong kind (non-whole int, unknown enum id, non-0/1 bool) or a value that breaks a
        // constraint returns false, changes and fires nothing, and names the key (constraints also name otherKey).
        // Fires OnAttributeChanged once for the changed attribute (not at all if the value is unchanged).
        bool Set(Dia::Core::StringCRC key, const Json::Value& value, ParamError& err);

        // Defaults plus `packs` in order (ApplyPack semantics), atomic: the whole new state is built aside and swapped
        // in only on success; on failure nothing changes (state, schema, hash, values, observers) and `warn` is
        // untouched. The result equals a fresh Load plus the same ApplyPack calls. Values from Set are discarded.
        // Modifiers (and their handles) are kept for attributes that still exist; modifiers on attributes the new
        // sidecar removes are dropped with one warning (key = the attribute). Observers fire once per surviving
        // attribute whose resolved value changed, after the swap; added or removed attributes fire nothing.
        // Map keys come from the IMapKeySource given to Load (non-owning; it must outlive this object). Called on
        // a file that is not loaded, it behaves like Load (with no map key source) followed by the packs.
        bool Reload(const char* sidecarText, const Json::Value* const* packs, uint32_t packCount,
                    ParamWarnings& warn, ParamError& err);

        // Full: envelope (format, version, basedOn) plus every parameter: floats and ints as numbers, bools as
        // true/false, enum ids as strings, a map as a nested object keyed by map key. Diff: the envelope plus only
        // the values whose bits differ from their sidecar default (empty diff = envelope only), so it is a pack.
        // Floats are written as the shortest decimal that reads back as the same float, so ApplyPack(ToJson(Diff))
        // over a fresh Load reproduces every base value bit for bit. A Json::Value object does not keep member
        // order (jsoncpp sorts keys): schema order is not observable in the result. Unloaded: an empty object.
        // Caveat: jsoncpp's text writer prints -0.0 as "-0", which its reader parses back as +0, so a -0.0 base
        // value survives the in-memory Json::Value but not a text round trip.
        Json::Value ToJson(JsonShape shape) const;

        // FNV-1a 64 over the base values in schema order: each value's IEEE-754 float32 bit pattern as 4
        // little-endian bytes, -0.0 normalised to +0.0. Modifiers are excluded. Not loaded: the empty-input hash.
        uint64_t Hash() const;

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
        const IMapKeySource* mMapKeys = nullptr;   // non-owning; remembered from Load for Reload
        Dia::Core::Containers::String128 mSidecarName;
        bool            mLoaded = false;
    };
}
