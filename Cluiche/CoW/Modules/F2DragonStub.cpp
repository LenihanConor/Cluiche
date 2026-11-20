#include "F2DragonStub.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

namespace {

struct BandSpec { F2Band band; float min; float max; };
constexpr BandSpec kBandRanges[] = {
    { F2Band::None,     0.0f,   5.0f },
    { F2Band::Low,      10.0f,  40.0f },
    { F2Band::Baseline, 40.0f,  60.0f },
    { F2Band::Medium,   60.0f,  100.0f },
    { F2Band::High,     110.0f, 160.0f },
    { F2Band::Apex,     170.0f, 200.0f },
};

// Builds the dragon-stub sidecar (dia.value_schema v1, kind: parameter entries with bands) as JSON text and loads it
// through the sidecar path. There is no schema asset file for this stub.
Dia::Attribute::AttributeSchema BuildSchema()
{
    Json::Value root;
    root["format"] = "dia.value_schema";
    root["version"] = 1;
    root["for"] = "f2_dragon_stub";
    Json::Value entries(Json::arrayValue);
    for (int a = 0; a < kF2DragonAttrCount; ++a)
    {
        Json::Value attr;
        attr["key"] = F2DragonStub::AttrName(static_cast<F2DragonAttr>(a));
        attr["kind"] = "parameter";
        attr["label"] = F2DragonStub::AttrName(static_cast<F2DragonAttr>(a));
        attr["description"] = "F2 dragon stub attribute";
        attr["type"] = "float";
        attr["min"] = 0.0;
        attr["max"] = 200.0;
        attr["default"] = 100.0;
        Json::Value bands;
        for (const BandSpec& b : kBandRanges)
        {
            Json::Value range(Json::arrayValue);
            range.append(b.min);
            range.append(b.max);
            bands[F2DragonStub::BandName(b.band)] = range;
        }
        attr["bands"] = bands;
        entries.append(attr);
    }
    root["entries"] = entries;

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const std::string text = Json::writeString(writer, root);

    const Dia::ValueSchema::LoadResult loaded = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(text.c_str(), "f2_dragon_stub.schema.json");
    Dia::Attribute::AttributeSchema schema;
    Dia::Attribute::SchemaError err;
    if (!loaded.mOk)
    {
        DIA_LOG_ERROR("CoW", "F2DragonStub: sidecar failed to load: %s", loaded.mFailure.mReason.AsCStr());
        return schema;
    }
    if (!Dia::Attribute::AttributeSchema::FromValueSchema(loaded, nullptr, schema, err))
    {
        DIA_LOG_ERROR("CoW", "F2DragonStub: schema build failed at '%s': %s", err.key.AsCStr(), err.reason.AsCStr());
    }
    return schema;
}

} // namespace

//-----------------------------------------------------------------------------
const char* F2DragonStub::AttrName(F2DragonAttr attr)
{
    switch (attr)
    {
    case F2DragonAttr::Health:    return "health";
    case F2DragonAttr::Strength:  return "strength";
    case F2DragonAttr::Agility:   return "agility";
    case F2DragonAttr::Fertility: return "fertility";
    }
    return "";
}

//-----------------------------------------------------------------------------
const char* F2DragonStub::BandName(F2Band band)
{
    switch (band)
    {
    case F2Band::None:     return "NONE";
    case F2Band::Low:      return "LOW";
    case F2Band::Baseline: return "BASELINE";
    case F2Band::Medium:   return "MEDIUM";
    case F2Band::High:     return "HIGH";
    case F2Band::Apex:     return "APEX";
    }
    return "";
}

//-----------------------------------------------------------------------------
F2DragonStub::F2DragonStub()
    : mSchema(BuildSchema())
{
    mSet.InitializeFromSchema(mSchema);
}

//-----------------------------------------------------------------------------
void F2DragonStub::Resolve()
{
    Dia::Core::Containers::DynamicArrayC<Dia::Attribute::BandValue, Dia::Attribute::AttributeSet::kMaxBandValuesPerCall> values;
    for (int a = 0; a < kF2DragonAttrCount; ++a)
    {
        Dia::Attribute::BandValue bv;
        bv.attribute_name = Dia::Core::StringCRC(AttrName(static_cast<F2DragonAttr>(a)));
        bv.band_name = Dia::Core::StringCRC(BandName(mRecord.bands[a]));
        values.Add(bv);
    }
    ++mInvocations;   // the only call site of the resolver
    mSet.InitializeFromBands(mSchema, values);
    mResolvedVersion = mRecord.bandSchemaVersion;
}

//-----------------------------------------------------------------------------
void F2DragonStub::Load(const F2DragonRecord& record)
{
    mRecord = record;
    Resolve();
    mLoaded = true;
}

//-----------------------------------------------------------------------------
bool F2DragonStub::ReloadIfVersionChanged()
{
    if (!VersionChanged())
        return false;
    Resolve();
    return true;
}

//-----------------------------------------------------------------------------
float F2DragonStub::Read(F2DragonAttr attr)
{
    if (mReResolveOnRead && mLoaded)
        Resolve();
    return mSet.GetBaseValue(Dia::Core::StringCRC(AttrName(attr)));
}

//-----------------------------------------------------------------------------
bool F2DragonStub::GetBandRange(F2DragonAttr attr, F2Band band, float& outMin, float& outMax) const
{
    const Dia::Attribute::AttributeDefinition* def = mSchema.FindAttribute(Dia::Core::StringCRC(AttrName(attr)));
    if (!def)
        return false;
    const Dia::Attribute::BandRange* range = def->bands.TryGetItemConst(Dia::Core::StringCRC(BandName(band)));
    if (!range)
        return false;
    outMin = range->min;
    outMax = range->max;
    return true;
}

} // namespace CoW
