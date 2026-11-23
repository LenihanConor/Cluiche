// Save codec, CoW half: sidecar ranges, params_hash check, and the pending-step blob.
// The data half (header, arrays, Validate, palette) lives in DraoiHexWorld/HexWorldSaveCodec.h and is called, not copied.
#pragma once

#include <stdint.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DraoiHexWorld/HexWorldSaveCodec.h>
#include <Shared/World/WorldSimNames.h>

namespace CoW {
namespace WorldStep {

// A saved field as the system declares it. The range comes from the sidecars, never from code.
struct FieldDecl
{
    const char* name;                         // snake_case; also the array key in the record
    Draoi::HexWorld::SavedKind kind;
    Draoi::HexWorld::SavedScope scope;
};

// Fills out[0..count) from the sidecar schema. A field's range is read from the layer entry
// `cow.<sys>.layers/<field>`; a field with no layer entry takes it from the state entry
// `cow.<sys>_state/<field>`. TSchema is ValueSchemaRegistry or ValueSchemaService (anything with Find(StringCRC)).
// false + err naming the field when neither entry exists or the entry has no min and max.
template <class TSchema>
bool ResolveSavedFields(const TSchema& schema, const char* sys, const FieldDecl* decls, uint32_t count,
                        Draoi::HexWorld::SavedField* out, Draoi::HexWorld::SaveError& err)
{
    const World::Name format = World::StateFormat(sys);
    for (uint32_t i = 0; i < count; ++i)
    {
        const FieldDecl& d = decls[i];
        const World::Name layerKey = World::LayerSchemaKey(sys, d.name);
        const auto* info = layerKey.IsEmpty() ? nullptr : schema.Find(Dia::Core::StringCRC(layerKey.c_str()));
        if (info == nullptr && !format.IsEmpty())
        {
            info = schema.Find(Dia::Core::StringCRC(format.c_str()), Dia::Core::StringCRC(d.name));
        }
        if (info == nullptr)
        {
            err.field.SetText(d.name);
            err.reason.SetText("no layer or state sidecar entry gives this field a range");
            return false;
        }
        if (!info->mHasMin || !info->mHasMax)
        {
            err.field.SetText(d.name);
            err.reason.SetText("sidecar entry has no min and max");
            return false;
        }
        out[i].name = d.name;
        out[i].kind = d.kind;
        out[i].scope = d.scope;
        out[i].min = info->mMin;
        out[i].max = info->mMax;
    }
    return true;
}

// true when the record's params_hash equals the current ParameterFile::Hash(). A record that is not ready
// or has no params_hash does not match.
bool ParamsHashMatches(const Json::Value& record, uint64_t currentHash);

}  // namespace WorldStep
}  // namespace CoW
