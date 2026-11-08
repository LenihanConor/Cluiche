// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md (Presets, AC19)
#include <Shared/WorldGeneration/WorldShapePreset.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace CoW {
namespace WorldGeneration {

namespace {

using Dia::Core::StringCRC;

void AddWarning(RuleWarnings& warnings, const char* prefix, const char* key)
{
    if (warnings.IsFull())
    {
        return;
    }
    if (warnings.Size() == warnings.Capacity() - 1)
    {
        warnings.Add(StringCRC("warnings:truncated"));
        return;
    }
    char text[StringCRC::kStringLength];
    std::snprintf(text, sizeof(text), "%s:%s", prefix, key);
    warnings.Add(StringCRC(text));
}

PresetResult Fail(PresetStatus status, std::string message, std::string path = {})
{
    PresetResult r;
    r.status = status;
    r.message = std::move(message);
    r.path = std::move(path);
    return r;
}

// One slider: key, kind, and the bounds used for range warnings (Clamp() enforces the same bounds).
enum class Kind : uint8_t { Float, UInt8 };
struct ParamKey
{
    const char* key;
    Kind        kind;
    float       lo;
    float       hi;
    float WorldShapeParams::* fieldF;
    uint8_t WorldShapeParams::* fieldU;
};

const ParamKey kParamKeys[] =
{
    { "landFraction",      Kind::Float, 0.20f, 0.65f,  &WorldShapeParams::landFraction,      nullptr },
    { "continents",        Kind::UInt8, 1.0f,  3.0f,   nullptr, &WorldShapeParams::continents },
    { "islands",           Kind::UInt8, 0.0f,  6.0f,   nullptr, &WorldShapeParams::islands },
    { "continentIslands",  Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::continentIslands,  nullptr },
    { "peninsulas",        Kind::UInt8, 0.0f,  3.0f,   nullptr, &WorldShapeParams::peninsulas },
    { "coastline",         Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::coastline,         nullptr },
    { "mountainousness",   Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::mountainousness,   nullptr },
    { "spineToRing",       Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::spineToRing,       nullptr },
    { "orientationDeg",    Kind::Float, 0.0f,  360.0f, &WorldShapeParams::orientationDeg,    nullptr },   // valid range is [0, 360)
    { "passes",            Kind::UInt8, 0.0f,  4.0f,   nullptr, &WorldShapeParams::passes },
    { "barrier",           Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::barrier,           nullptr },
    { "ruggedness",        Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::ruggedness,        nullptr },
    { "rivers",            Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::rivers,            nullptr },
    { "valleyDepth",       Kind::Float, 0.0f,  1.0f,   &WorldShapeParams::valleyDepth,       nullptr },
    { "lakes",             Kind::UInt8, 0.0f,  12.0f,  nullptr, &WorldShapeParams::lakes },
    { "latitudeNorthDeg",  Kind::Float, -90.0f, 90.0f, &WorldShapeParams::latitudeNorthDeg,  nullptr },
    { "latitudeSouthDeg",  Kind::Float, -90.0f, 90.0f, &WorldShapeParams::latitudeSouthDeg,  nullptr },
    { "temperatureOffset", Kind::Float, -0.3f, 0.3f,   &WorldShapeParams::temperatureOffset, nullptr },
    { "moistureOffset",    Kind::Float, -0.3f, 0.3f,   &WorldShapeParams::moistureOffset,    nullptr },
};
constexpr const char* kBarrierFormKey = "barrierForm";

bool IsParamKey(const std::string& key)
{
    if (key == kBarrierFormKey)
    {
        return true;
    }
    for (const ParamKey& p : kParamKeys)
    {
        if (key == p.key)
        {
            return true;
        }
    }
    return false;
}

// Shortest %.{1..9}g text that reads back as the same float, as a double for jsoncpp (written at precision 9).
double ShortestFloat(float f)
{
    char text[32];
    for (int digits = 1; digits <= 9; ++digits)
    {
        std::snprintf(text, sizeof(text), "%.*g", digits, static_cast<double>(f));
        if (static_cast<float>(std::strtod(text, nullptr)) == f)
        {
            break;
        }
    }
    return std::strtod(text, nullptr);
}

std::string WriteJson(const Json::Value& v)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "  ";
    b["precision"] = 9;
    return Json::writeString(b, v) + "\n";
}

} // namespace

bool IsValidPresetId(const std::string& id)
{
    if (id.empty() || id.size() > kMaxPresetIdLength)
    {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

std::string PresetFilePath(const std::string& directory, const std::string& id)
{
    return (std::filesystem::path(directory) / (id + kWorldShapePresetExtension)).string();
}

PresetResult ParsePreset(const Json::Value& root, const std::string& expectedId, WorldShapePreset& out, RuleWarnings& warnings)
{
    out = WorldShapePreset{};

    if (!root.isObject())
    {
        return Fail(PresetStatus::InvalidFormat, "root is not a JSON object");
    }
    if (!root.isMember("format") || !root["format"].isString() || root["format"].asString() != kWorldShapePresetFormat)
    {
        return Fail(PresetStatus::InvalidFormat, std::string("\"format\" must be \"") + kWorldShapePresetFormat + "\"");
    }
    if (!root.isMember("version") || !root["version"].isIntegral())
    {
        return Fail(PresetStatus::InvalidFormat, "\"version\" missing or not an integer");
    }
    const Json::Int64 version = root["version"].asInt64();
    if (version < 1 || version > static_cast<Json::Int64>(kWorldShapePresetVersion))
    {
        return Fail(PresetStatus::UnsupportedVersion, "unsupported preset version " + std::to_string(version) +
                    " (supported: " + std::to_string(kWorldShapePresetVersion) + ")");
    }
    if (!root.isMember("id") || !root["id"].isString())
    {
        return Fail(PresetStatus::InvalidId, "\"id\" missing or not a string");
    }
    const std::string id = root["id"].asString();
    if (!IsValidPresetId(id))
    {
        return Fail(PresetStatus::InvalidId, "id \"" + id + "\" does not match [a-z0-9_]{1,32}");
    }
    if (!expectedId.empty() && id != expectedId)
    {
        return Fail(PresetStatus::IdMismatch, "id \"" + id + "\" does not match expected \"" + expectedId + "\"");
    }
    if (!root.isMember("params") || !root["params"].isObject())
    {
        return Fail(PresetStatus::InvalidFormat, "\"params\" missing or not an object");
    }

    WorldShapePreset preset;
    preset.id = id;
    preset.version = static_cast<uint32_t>(version);

    if (root.isMember("displayName") && root["displayName"].isString())
    {
        preset.displayName = root["displayName"].asString();
    }
    else
    {
        AddWarning(warnings, root.isMember("displayName") ? "invalid" : "missing", "displayName");
        preset.displayName = id;
    }
    if (root.isMember("description") && root["description"].isString())
    {
        preset.description = root["description"].asString();
    }
    else if (root.isMember("description"))
    {
        AddWarning(warnings, "invalid", "description");
    }

    const Json::Value& params = root["params"];

    for (const std::string& key : params.getMemberNames())
    {
        if (!IsParamKey(key))
        {
            AddWarning(warnings, "unknown", key.c_str());
        }
    }

    WorldShapeParams p;   // defaults
    for (const ParamKey& pk : kParamKeys)
    {
        if (!params.isMember(pk.key))
        {
            AddWarning(warnings, "missing", pk.key);
            continue;
        }
        const Json::Value& jv = params[pk.key];
        if (!jv.isNumeric())
        {
            AddWarning(warnings, "invalid", pk.key);
            continue;
        }
        const double d = jv.asDouble();
        if (!std::isfinite(d))
        {
            AddWarning(warnings, "range", pk.key);
            continue;
        }
        if (pk.kind == Kind::Float)
        {
            const float f = static_cast<float>(d);
            const bool outOfRange = std::strcmp(pk.key, "orientationDeg") == 0 ? (f < 0.0f || !(f < 360.0f)) : (f < pk.lo || f > pk.hi);
            if (outOfRange)
            {
                AddWarning(warnings, "range", pk.key);
            }
            p.*(pk.fieldF) = f;   // clamped by Clamp() below
        }
        else
        {
            const double r = std::round(d);
            if (r != d || r < pk.lo || r > pk.hi)
            {
                AddWarning(warnings, "range", pk.key);
            }
            const double c = std::min<double>(std::max<double>(r, pk.lo), pk.hi);
            p.*(pk.fieldU) = static_cast<uint8_t>(c);
        }
    }

    if (!params.isMember(kBarrierFormKey))
    {
        AddWarning(warnings, "missing", kBarrierFormKey);
    }
    else
    {
        const Json::Value& jv = params[kBarrierFormKey];
        const std::string form = jv.isString() ? jv.asString() : std::string();
        if (form == "Plateau")
        {
            p.barrierForm = BarrierForm::Plateau;
        }
        else if (form == "VolcanicChain")
        {
            p.barrierForm = BarrierForm::VolcanicChain;
        }
        else
        {
            return Fail(PresetStatus::UnknownBarrierForm,
                        jv.isString() ? "unknown barrierForm \"" + form + "\"" : std::string("barrierForm is not a string"));
        }
    }

    p.Clamp();
    preset.params = p;
    out = preset;
    return PresetResult{};
}

Json::Value PresetToJson(const WorldShapePreset& preset)
{
    WorldShapeParams p = preset.params;
    p.Clamp();

    Json::Value root(Json::objectValue);
    root["format"] = kWorldShapePresetFormat;
    root["version"] = static_cast<Json::UInt>(kWorldShapePresetVersion);
    root["id"] = preset.id;
    root["displayName"] = preset.displayName;
    root["description"] = preset.description;

    Json::Value params(Json::objectValue);
    for (const ParamKey& pk : kParamKeys)
    {
        if (pk.kind == Kind::Float)
        {
            params[pk.key] = ShortestFloat(p.*(pk.fieldF));
        }
        else
        {
            params[pk.key] = static_cast<Json::UInt>(p.*(pk.fieldU));
        }
    }
    params[kBarrierFormKey] = BarrierFormName(p.barrierForm);
    root["params"] = params;
    return root;
}

PresetResult LoadPreset(const std::string& directory, const std::string& id, WorldShapePreset& out, RuleWarnings& warnings)
{
    out = WorldShapePreset{};
    if (!IsValidPresetId(id))
    {
        return Fail(PresetStatus::InvalidId, "id \"" + id + "\" does not match [a-z0-9_]{1,32}");
    }
    const std::string path = PresetFilePath(directory, id);
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return Fail(PresetStatus::FileNotFound, "preset file not found: " + path, path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    const std::string text = buffer.str();
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(text.data(), text.data() + text.size(), &root, &errors))
    {
        return Fail(PresetStatus::ParseError, "JSON parse error in " + path + ": " + errors, path);
    }
    PresetResult r = ParsePreset(root, id, out, warnings);
    r.path = path;
    return r;
}

PresetResult SavePreset(const std::string& directory, const WorldShapePreset& preset, bool overwrite)
{
    if (!IsValidPresetId(preset.id))
    {
        return Fail(PresetStatus::InvalidId, "id \"" + preset.id + "\" does not match [a-z0-9_]{1,32}");
    }
    const std::string path = PresetFilePath(directory, preset.id);
    std::error_code ec;
    if (!overwrite && std::filesystem::exists(path, ec))
    {
        return Fail(PresetStatus::AlreadyExists, "preset already exists (overwrite not set): " + path, path);
    }
    std::filesystem::create_directories(directory, ec);
    if (ec)
    {
        return Fail(PresetStatus::IoError, "cannot create directory " + directory + ": " + ec.message(), path);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return Fail(PresetStatus::IoError, "cannot open for writing: " + path, path);
    }
    file << WriteJson(PresetToJson(preset));
    file.close();
    if (!file)
    {
        return Fail(PresetStatus::IoError, "write failed: " + path, path);
    }
    PresetResult r;
    r.path = path;
    return r;
}

std::vector<PresetInfo> ListPresets(const std::string& directory)
{
    std::vector<PresetInfo> infos;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec))
    {
        return infos;
    }
    const std::string ext = kWorldShapePresetExtension;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (!entry.is_regular_file(ec))
        {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.size() <= ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0)
        {
            continue;
        }
        const std::string id = name.substr(0, name.size() - ext.size());
        WorldShapePreset preset;
        RuleWarnings warnings;
        if (!LoadPreset(directory, id, preset, warnings).Ok())
        {
            continue;
        }
        PresetInfo info;
        info.id = preset.id;
        info.displayName = preset.displayName;
        info.description = preset.description;
        info.version = preset.version;
        info.sourcePath = entry.path().string();
        infos.push_back(std::move(info));
    }
    std::sort(infos.begin(), infos.end(), [](const PresetInfo& a, const PresetInfo& b) { return a.id < b.id; });
    return infos;
}

} // namespace WorldGeneration
} // namespace CoW
