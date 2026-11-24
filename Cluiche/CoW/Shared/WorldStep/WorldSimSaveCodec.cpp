#include <Shared/WorldStep/WorldSimSaveCodec.h>

#include <string>

namespace CoW {
namespace WorldStep {

bool ParamsHashMatches(const Json::Value& record, uint64_t currentHash)
{
    if (!record.isObject() || !record.isMember("params_hash") || !record["params_hash"].isUInt64()) { return false; }
    return record["params_hash"].asUInt64() == currentHash;
}

namespace {

bool Fail(Draoi::HexWorld::SaveError& err, const char* field, const char* reason)
{
    err.field.SetText(field);
    err.reason.SetText(reason);
    return false;
}

// Header members the blob inherits from its committed record.
const char* const kInherited[] = { "width", "height", "params_hash", "basin_count", "world_hash" };

}  // namespace

void WritePendingInvalid(Json::Value& blob)
{
    blob["pending_version"] = Json::UInt(kPendingVersion);
    blob["valid"] = false;
}

void WritePendingHeader(Json::Value& blob, uint32_t stepDay, uint32_t cellCount)
{
    blob["pending_version"] = Json::UInt(kPendingVersion);
    blob["valid"] = true;
    blob["step_day"] = Json::UInt(stepDay);
    blob["cell_count"] = Json::UInt(cellCount);
}

bool ValidatePendingBlob(const Json::Value& blob, const Json::Value* committed, const RecordSpec& spec,
                         Draoi::HexWorld::SaveError& err)
{
    if (!blob.isObject()) { return Fail(err, "pending_version", "pending blob must be an object"); }
    if (!blob.isMember("pending_version") || !blob["pending_version"].isUInt()
        || blob["pending_version"].asUInt() != kPendingVersion)
    {
        return Fail(err, "pending_version", "missing or unsupported pending_version");
    }
    if (!blob.isMember("valid") || !blob["valid"].isBool()) { return Fail(err, "valid", "required boolean missing"); }
    if (!blob["valid"].asBool()) { return true; }

    if (blob.isMember("biome_palette")) { return Fail(err, "biome_palette", "a pending blob must not repeat the palette"); }
    if (committed == nullptr || !committed->isObject() || !(*committed)["ready"].isBool() || !(*committed)["ready"].asBool())
    {
        return Fail(err, spec.hasBiome ? "biome_palette" : "committed_record",
                    "a pending blob needs its committed record to decode against");
    }

    // The blob decodes against its committed record: inherit the header and run the Draoi checks over the view.
    Json::Value view(Json::objectValue);
    view["ready"] = true;
    for (const char* key : kInherited)
    {
        if ((*committed).isMember(key)) { view[key] = (*committed)[key]; }
    }
    const Json::Value::Members names = blob.getMemberNames();
    for (const std::string& name : names) { view[name] = blob[name]; }
    if (!Draoi::HexWorld::ValidatePending(view, spec.fields, spec.fieldCount, spec.caps, err)) { return false; }

    if (spec.hasBiome && spec.pendingBiome)
    {
        if (!committed->isMember("biome_palette") || !(*committed)["biome_palette"].isArray())
        {
            return Fail(err, "biome_palette", "committed record has no palette");
        }
        const uint32_t paletteSize = (*committed)["biome_palette"].size();
        if (!blob.isMember("biome") || !blob["biome"].isArray()) { return Fail(err, "biome", "required array missing"); }
        const Json::Value& cells = blob["biome"];
        if (!view["cell_count"].isUInt() || cells.size() != view["cell_count"].asUInt())
        {
            return Fail(err, "biome", "array length does not match cell_count");
        }
        for (uint32_t i = 0; i < cells.size(); ++i)
        {
            if (!cells[i].isUInt() || cells[i].asUInt() >= paletteSize)
            {
                return Fail(err, "biome", "element is outside the committed palette");
            }
        }
    }
    return true;
}

}  // namespace WorldStep
}  // namespace CoW
