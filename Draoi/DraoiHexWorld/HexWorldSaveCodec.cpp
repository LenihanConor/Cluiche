#include <DraoiHexWorld/HexWorldSaveCodec.h>

#include <DiaObservation/Log/DiaLog.h>

#include <cmath>
#include <cstdio>

namespace Draoi {
namespace HexWorld {

namespace {

bool Fail(SaveError& err, const char* field, const char* fmt, double a = 0.0, double b = 0.0)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), fmt, a, b);
    err.field.SetText(field);
    err.reason.SetText(buf);
    return false;
}

bool ReadUInt(const Json::Value& root, const char* key, uint64_t maxValue, uint64_t& out, SaveError& err)
{
    if (!root.isMember(key))
        return Fail(err, key, "required key missing");
    const Json::Value& v = root[key];
    if (!v.isUInt64())
        return Fail(err, key, "must be a non-negative integer");
    out = v.asUInt64();
    if (out > maxValue)
        return Fail(err, key, "value too large");
    return true;
}

bool ValidateValue(const SavedField& f, const Json::Value& v, uint32_t index, SaveError& err)
{
    if (!v.isNumeric())
        return Fail(err, f.name, "element %.0f is not a number", index);
    const double d = v.asDouble();
    if (!std::isfinite(d))
        return Fail(err, f.name, "element %.0f is not finite", index);

    if (f.kind != SavedKind::F32)
    {
        if (std::floor(d) != d)
            return Fail(err, f.name, "element %.0f is not an integer", index);
        const double kindMax = (f.kind == SavedKind::U8) ? 255.0 : 65535.0;
        if (d < 0.0 || d > kindMax)
            return Fail(err, f.name, "element %.0f is outside the integer kind range", index);
    }
    if (d < f.min || d > f.max)
        return Fail(err, f.name, "element %.0f is outside the allowed range", index);
    return true;
}

} // namespace

void WriteHeader(Json::Value& root, const SaveHeader& h)
{
    root["ready"]       = true;
    root["width"]       = Json::UInt(h.width);
    root["height"]      = Json::UInt(h.height);
    root["cell_count"]  = Json::UInt(h.cellCount);
    root["step_day"]    = Json::Int(h.stepDay);
    root["params_hash"] = Json::UInt64(h.paramsHash);
    root["basin_count"] = Json::UInt(h.basinCount);
    root["world_hash"]  = Json::UInt64(h.worldHash);
}

void WriteNotReady(Json::Value& root)
{
    root["ready"] = false;
}

void WriteArray(Json::Value& root, const char* name, const float* values, uint32_t count)
{
    Json::Value arr(Json::arrayValue);
    for (uint32_t i = 0; i < count; ++i)
    {
        double d = static_cast<double>(values[i]);
        if (d == 0.0)
            d = 0.0;   // -0.0 -> +0.0
        arr.append(d);
    }
    root[name] = arr;
}

std::string Serialize(const Json::Value& root)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    b["precision"] = 17;
    return Json::writeString(b, root);
}

bool Validate(const Json::Value& root, const SavedField* fields, uint32_t fieldCount, const Caps& caps, SaveError& err)
{
    if (!root.isObject() || !root.isMember("ready"))
        return Fail(err, "ready", "required key missing");
    if (!root["ready"].isBool())
        return Fail(err, "ready", "must be a boolean");
    if (!root["ready"].asBool())
        return true;

    uint64_t width, height, cellCount, basinCount, paramsHash, worldHash;
    if (!ReadUInt(root, "width", 0xFFFFFFFFull, width, err)) return false;
    if (!ReadUInt(root, "height", 0xFFFFFFFFull, height, err)) return false;
    if (!ReadUInt(root, "cell_count", 0xFFFFFFFFull, cellCount, err)) return false;
    if (!root.isMember("step_day"))
        return Fail(err, "step_day", "required key missing");
    if (!ReadUInt(root, "params_hash", ~0ull, paramsHash, err)) return false;
    if (!ReadUInt(root, "basin_count", 0xFFFFFFFFull, basinCount, err)) return false;
    if (!ReadUInt(root, "world_hash", ~0ull, worldHash, err)) return false;

    if (cellCount != width * height)
        return Fail(err, "cell_count", "cell_count does not equal width * height");
    if (cellCount == 0)
        return Fail(err, "cell_count", "cell_count must be non-zero");
    if (cellCount > kMaxSaveCells)
        return Fail(err, "cell_count", "cell_count exceeds %.0f", kMaxSaveCells);
    if (basinCount > caps.maxBasins)
        return Fail(err, "basin_count", "basin_count exceeds the cap of %.0f", caps.maxBasins);

    const Json::Value& day = root["step_day"];
    if (!day.isIntegral())
        return Fail(err, "step_day", "must be an integer");
    if (day.asDouble() < 0.0)
        return Fail(err, "step_day", "must not be negative");

    for (uint32_t f = 0; f < fieldCount; ++f)
    {
        const SavedField& field = fields[f];
        if (!root.isMember(field.name))
            return Fail(err, field.name, "required array missing");
        const Json::Value& arr = root[field.name];
        if (!arr.isArray())
            return Fail(err, field.name, "must be an array");
        const uint64_t expected = (field.scope == SavedScope::PerCell) ? cellCount : basinCount;
        if (arr.size() != expected)
            return Fail(err, field.name, "array length %.0f does not match expected %.0f", static_cast<double>(arr.size()), static_cast<double>(expected));
        for (uint32_t i = 0; i < arr.size(); ++i)
            if (!ValidateValue(field, arr[i], i, err))
                return false;
    }
    return true;
}

bool ValidatePending(const Json::Value& blob, const SavedField* fields, uint32_t fieldCount, const Caps& caps, SaveError& err)
{
    return Validate(blob, fields, fieldCount, caps, err);
}

void WriteBiomeCells(Json::Value& root, const BiomeRegistry& reg, const BiomeIndex* cells, uint32_t count)
{
    Json::Value palette(Json::arrayValue);
    for (uint32_t i = 0; i < reg.Count(); ++i)
        palette.append(reg.IdText(static_cast<BiomeIndex>(i)));
    root["biome_palette"]      = palette;
    root["biome_palette_hash"] = Json::UInt64(reg.Hash());

    Json::Value arr(Json::arrayValue);
    for (uint32_t i = 0; i < count; ++i)
        arr.append(Json::UInt(cells[i]));
    root["biome"] = arr;
}

uint64_t StoredBiomeHash(const Json::Value& root)
{
    if (!root.isObject() || !root.isMember("biome_palette_hash") || !root["biome_palette_hash"].isUInt64())
        return 0;
    return root["biome_palette_hash"].asUInt64();
}

bool ReadBiomeCells(const Json::Value& root, const BiomeRegistry& reg, BiomeIndex* outCells, uint32_t count, SaveError& err)
{
    if (!root.isObject() || !root.isMember("biome_palette"))
        return Fail(err, "biome_palette", "required array missing");
    const Json::Value& palette = root["biome_palette"];
    if (!palette.isArray())
        return Fail(err, "biome_palette", "must be an array");
    if (palette.size() > BiomeRegistry::kMaxBiomes)
        return Fail(err, "biome_palette", "palette has too many entries");
    for (uint32_t i = 0; i < palette.size(); ++i)
        if (!palette[i].isString())
            return Fail(err, "biome_palette", "entry %.0f is not a string", i);

    if (!root.isMember("biome"))
        return Fail(err, "biome", "required array missing");
    const Json::Value& cells = root["biome"];
    if (!cells.isArray())
        return Fail(err, "biome", "must be an array");
    if (cells.size() != count)
        return Fail(err, "biome", "array length %.0f does not match expected %.0f", static_cast<double>(cells.size()), static_cast<double>(count));
    for (uint32_t i = 0; i < count; ++i)
    {
        const Json::Value& v = cells[i];
        if (!v.isUInt())
            return Fail(err, "biome", "element %.0f is not a non-negative integer", i);
        if (v.asUInt() >= palette.size())
            return Fail(err, "biome", "element %.0f is outside the palette", i);
    }

    // Remap each palette id once; ResolveOrFallback warns once per distinct unknown id.
    BiomeIndex remap[BiomeRegistry::kMaxBiomes] = {};
    bool allResolved = true;
    for (uint32_t i = 0; i < palette.size(); ++i)
    {
        const Dia::Core::StringCRC id(palette[i].asCString());
        if (reg.Find(id) == kNoBiome)
            allResolved = false;
        remap[i] = reg.ResolveOrFallback(id);
    }
    for (uint32_t i = 0; i < count; ++i)
        outCells[i] = remap[cells[i].asUInt()];

    const uint64_t stored = StoredBiomeHash(root);
    if (allResolved && stored != reg.Hash())
        DIA_LOG_INFO("HexWorld", "HexWorldSaveCodec: biome palette hash differs (stored %016llx, current %016llx); ids remapped cleanly",
                     static_cast<unsigned long long>(stored), static_cast<unsigned long long>(reg.Hash()));
    return true;
}

} // namespace HexWorld
} // namespace Draoi
