#include <Shared/Debug/ParamCommandHelper.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {
namespace Debug {

using Dia::Attribute::AttributeKind;

PresetPackLayer::PresetPackLayer(const char* sysToken)
    : mSysKnown(sysToken != nullptr && World::IsKnownToken(sysToken))
{
}

bool PresetPackLayer::AddUserPack(const char* packId, const Json::Value& body)
{
    if (packId == nullptr || *packId == '\0' || !body.isObject()) { return false; }
    mUserIds.push_back(packId);
    mUserBodies.push_back(body);
    return true;
}

bool PresetPackLayer::ApplyPreset(const char* presetId, const Json::Value& body)
{
    if (!mSysKnown || presetId == nullptr || *presetId == '\0' || !body.isObject()) { return false; }
    mPresetId = presetId;
    mPresetBody = body;
    mHasPreset = true;
    return true;
}

void PresetPackLayer::ClearPreset()
{
    mHasPreset = false;
    mPresetId.clear();
    mPresetBody = Json::Value();
}

void PresetPackLayer::Compose(ParamReloadResult& out) const
{
    out.packIds = mUserIds;
    out.packBodies = mUserBodies;
    if (mHasPreset)
    {
        out.packIds.push_back(mPresetId);
        out.packBodies.push_back(mPresetBody);
    }
}

ParamCommandHelper::ParamCommandHelper(const char* sysToken, Dia::Attribute::ParameterFile& file)
    : mName(World::CommandName(sysToken, "params"))
    , mFile(file)
{
    mPackIds[0][0] = '\0';
}

SimCommandEntry ParamCommandHelper::BuildEntry()
{
    static const Dia::API::ValueParam kParams[] = {
        { "action", "enum", true, "What to do with the parameters", nullptr, false, 0.0f, 0.0f, "get|list|set|reload|save_defaults" },
        { "key", "string", false, "Parameter key (get, set)" },
        { "prefix", "string", false, "Only parameters whose key starts with this (list)" },
        { "value", "string", false, "New value (set): a number, bool or enum id" } };

    SimCommandEntry entry;
    entry.name = mName.c_str();
    entry.kind = EntryKind::kCommand;
    entry.help.description = "Read and change this system's tunable parameters (get, list, set, reload, save_defaults)";
    entry.help.category = "worldsim";
    entry.help.owner = "ParamCommandHelper";
    entry.help.params = kParams;
    entry.help.paramCount = sizeof(kParams) / sizeof(kParams[0]);
    entry.handler = [this](const Json::Value& args) { return Handle(args); };
    return entry;
}

Json::Value ParamCommandHelper::Error(const char* text, const char* key)
{
    Json::Value e(Json::objectValue);
    e["success"] = false;
    char buf[256];
    if (key != nullptr) { snprintf(buf, sizeof(buf), "%s: '%s'", text, key); }
    else { snprintf(buf, sizeof(buf), "%s", text); }
    e["error"] = buf;
    return e;
}

Json::Value ParamCommandHelper::Handle(const Json::Value& args)
{
    if (!args.isObject() || !args["action"].isString()) { return Error("missing 'action' (get, list, set, reload, save_defaults)", nullptr); }
    const std::string action = args["action"].asString();
    if (action == "get") { return Get(args); }
    if (action == "list") { return List(args); }
    if (action == "set") { return SetAction(args); }
    if (action == "reload") { return ReloadAction(); }
    if (action == "save_defaults")
    {
        return Error("action not implemented yet", action.c_str());
    }
    return Error("unknown action", action.c_str());
}


int32_t ParamCommandHelper::FindIndex(const char* key) const
{
    const Dia::Attribute::AttributeSchema& schema = mFile.Schema();
    const Dia::Core::StringCRC crc(key);
    for (uint32_t i = 0; i < schema.GetAttributeCount(); ++i)
    {
        if (schema.GetAttributeByIndex(i).attribute_name == crc) { return static_cast<int32_t>(i); }
    }
    return -1;
}

uint8_t ParamCommandHelper::SourceOf(uint32_t index) const
{
    return index < mSources.size() ? mSources[index] : kSourceDefault;
}

void ParamCommandHelper::MarkKey(const char* key, uint8_t source)
{
    const int32_t index = FindIndex(key);
    if (index < 0) { return; }
    if (mSources.size() < mFile.Schema().GetAttributeCount()) { mSources.resize(mFile.Schema().GetAttributeCount(), kSourceDefault); }
    mSources[static_cast<uint32_t>(index)] = source;
}

void ParamCommandHelper::ClearSources()
{
    mSources.clear();
    mPackCount = 0;
}

bool ParamCommandHelper::NotePackApplied(const char* packId, const Json::Value& pack)
{
    if (mPackCount >= kMaxPacks || !pack.isObject())
    {
        DIA_LOG_WARNING("WorldSim", "%s: pack '%s' not recorded for value sources", mName.c_str(), packId != nullptr ? packId : "");
        return false;
    }
    const uint8_t source = static_cast<uint8_t>(mPackCount + 1);
    snprintf(mPackIds[mPackCount], kPackIdCapacity, "%s", packId != nullptr ? packId : "");
    ++mPackCount;
    const Json::Value::Members names = pack.getMemberNames();
    for (const std::string& name : names)
    {
        if (name == "format" || name == "version" || name == "basedOn") { continue; }
        const Json::Value& v = pack[name];
        if (v.isObject())
        {
            const Json::Value::Members sub = v.getMemberNames();   // a map: columns are "<key>.<mapKey>"
            for (const std::string& column : sub) { MarkKey((name + "." + column).c_str(), source); }
        }
        else { MarkKey(name.c_str(), source); }
    }
    return true;
}

namespace {

Json::Value ValueJson(const Dia::Attribute::AttributeSchema& schema, uint32_t index, float v)
{
    const Dia::Attribute::AttributeDefinition& def = schema.GetAttributeByIndex(index);
    switch (def.kind)
    {
    case AttributeKind::Int:   return Json::Value(static_cast<int>(v >= 0.0f ? v + 0.5f : v - 0.5f));
    case AttributeKind::Bool:  return Json::Value(v != 0.0f);
    case AttributeKind::Enum:
    {
        const int ordinal = static_cast<int>(v + 0.5f);
        if (ordinal >= 0 && static_cast<uint32_t>(ordinal) < def.enumIdCount)
        {
            return Json::Value(schema.GetEnumId(index, static_cast<unsigned int>(ordinal)).AsCStr());
        }
        return Json::Value(ordinal);
    }
    default:                   return Json::Value(static_cast<double>(v));
    }
}

const char* KindText(AttributeKind kind)
{
    switch (kind)
    {
    case AttributeKind::Int:  return "int";
    case AttributeKind::Bool: return "bool";
    case AttributeKind::Enum: return "enum";
    default:                  return "float";
    }
}

}  // namespace

Json::Value ParamCommandHelper::Describe(uint32_t index) const
{
    const Dia::Attribute::AttributeSchema& schema = mFile.Schema();
    const Dia::Attribute::AttributeDefinition& def = schema.GetAttributeByIndex(index);
    Json::Value r(Json::objectValue);
    r["key"] = def.fullKey.AsCStr();
    r["value"] = ValueJson(schema, index, mFile.Values().GetBaseValueByIndex(index));
    r["default"] = ValueJson(schema, index, def.default_value);
    r["min"] = static_cast<double>(def.minimum_value);
    r["max"] = static_cast<double>(def.maximum_value);
    r["kind"] = KindText(def.kind);
    const uint8_t source = SourceOf(index);
    if (source == kSourceDefault) { r["source"] = "default"; }
    else if (source == kSourceSet) { r["source"] = "set"; }
    else { r["source"] = std::string("pack:") + mPackIds[source - 1]; }
    return r;
}

Json::Value ParamCommandHelper::Get(const Json::Value& args) const
{
    if (!args["key"].isString()) { return Error("missing 'key'", nullptr); }
    const std::string key = args["key"].asString();
    const int32_t index = mFile.IsLoaded() ? FindIndex(key.c_str()) : -1;
    if (index < 0) { return Error("unknown parameter key", key.c_str()); }
    Json::Value r = Describe(static_cast<uint32_t>(index));
    r["success"] = true;
    return r;
}

Json::Value ParamCommandHelper::List(const Json::Value& args) const
{
    std::string prefix;
    if (args.isMember("prefix"))
    {
        if (!args["prefix"].isString()) { return Error("'prefix' must be a string", nullptr); }
        prefix = args["prefix"].asString();
    }
    Json::Value params(Json::arrayValue);
    if (mFile.IsLoaded())
    {
        const uint32_t count = mFile.Schema().GetAttributeCount();
        for (uint32_t i = 0; i < count; ++i)
        {
            const char* key = mFile.Schema().GetAttributeByIndex(i).fullKey.AsCStr();
            if (strncmp(key, prefix.c_str(), prefix.size()) != 0) { continue; }
            params.append(Describe(i));
        }
    }
    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["params"] = params;
    return r;
}

}  // namespace Debug
}  // namespace CoW

namespace CoW {
namespace Debug {

namespace {

bool ParseNumber(const Json::Value& v, double& out)
{
    if (v.isNumeric()) { out = v.asDouble(); return isfinite(out) != 0; }
    if (v.isBool()) { out = v.asBool() ? 1.0 : 0.0; return true; }
    if (!v.isString()) { return false; }
    const std::string text = v.asString();
    if (text.empty()) { return false; }
    char* end = nullptr;
    out = strtod(text.c_str(), &end);
    return end != nullptr && *end == '\0' && isfinite(out) != 0;
}

}  // namespace

Json::Value ParamCommandHelper::SetAction(const Json::Value& args)
{
    if (!args["key"].isString()) { return Error("missing 'key'", nullptr); }
    const std::string key = args["key"].asString();
    if (!args.isMember("value")) { return Error("missing 'value'", key.c_str()); }
    if (!mEnqueue) { return Error("no mutation queue wired, cannot set", key.c_str()); }
    const int32_t found = mFile.IsLoaded() ? FindIndex(key.c_str()) : -1;
    if (found < 0) { return Error("unknown parameter key", key.c_str()); }
    const uint32_t index = static_cast<uint32_t>(found);
    const Dia::Attribute::AttributeSchema& schema = mFile.Schema();
    const Dia::Attribute::AttributeDefinition& def = schema.GetAttributeByIndex(index);
    const Json::Value& raw = args["value"];

    // Validate against the schema without touching the file: kind, range, then constraints on a scratch copy.
    Json::Value normalised;
    float stored = 0.0f;
    double n = 0.0;
    switch (def.kind)
    {
    case AttributeKind::Enum:
    {
        if (!raw.isString()) { return Error("value must be an enum id", key.c_str()); }
        const std::string id = raw.asString();
        bool ok = false;
        for (uint32_t i = 0; i < def.enumIdCount; ++i)
        {
            if (id == schema.GetEnumId(index, i).AsCStr()) { stored = static_cast<float>(i); ok = true; break; }
        }
        if (!ok) { return Error("unknown enum id", key.c_str()); }
        normalised = Json::Value(id);
        break;
    }
    case AttributeKind::Bool:
    {
        bool b = false;
        if (raw.isString())
        {
            const std::string t = raw.asString();
            if (t == "true" || t == "1") { b = true; }
            else if (t != "false" && t != "0") { return Error("value must be true or false", key.c_str()); }
        }
        else if (!ParseNumber(raw, n) || (n != 0.0 && n != 1.0)) { return Error("value must be true or false", key.c_str()); }
        else { b = n != 0.0; }
        stored = b ? 1.0f : 0.0f;
        normalised = Json::Value(b);
        break;
    }
    default:
    {
        if (!ParseNumber(raw, n)) { return Error("value must be a number", key.c_str()); }
        if (def.kind == AttributeKind::Int && n != floor(n)) { return Error("value must be a whole number", key.c_str()); }
        if (n < static_cast<double>(def.minimum_value) || n > static_cast<double>(def.maximum_value))
        {
            return Error("value out of range", key.c_str());
        }
        stored = static_cast<float>(n);
        normalised = Json::Value(n);
        break;
    }
    }

    const uint32_t count = schema.GetAttributeCount();
    std::vector<float> candidate(count);
    for (uint32_t i = 0; i < count; ++i) { candidate[i] = mFile.Values().GetBaseValueByIndex(i); }
    candidate[index] = stored;
    Dia::Attribute::SchemaError cerr;
    if (!schema.CheckConstraints(candidate.data(), count, cerr))
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "value breaks a constraint (%s)", cerr.reason.AsCStr());
        return Error(buf, key.c_str());
    }

    ParamMutation m;
    m.kind = ParamMutation::Kind::kSet;
    snprintf(m.key, sizeof(m.key), "%s", key.c_str());
    m.value = normalised;
    if (!mEnqueue(m)) { return Error("mutation queue full, try again after the next step", key.c_str()); }

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["queued"] = true;
    r["key"] = key;
    r["value"] = normalised;
    const ParamDayInfo day = mDayProvider ? mDayProvider() : ParamDayInfo();
    r["effectiveDay"] = day.effectiveDay;
    if (day.deferLag > 0) { r["visibleDay"] = day.effectiveDay + day.deferLag; }
    return r;
}

Json::Value ParamCommandHelper::ReloadAction()
{
    if (!mEnqueue || !mReloadFn) { return Error("no reload wired, cannot reload", nullptr); }
    ParamReloadResult preview;
    if (!mReloadFn(false, preview)) { return Error(preview.error.empty() ? "reload failed" : preview.error.c_str(), nullptr); }
    ParamMutation m;
    m.kind = ParamMutation::Kind::kReload;
    if (!mEnqueue(m)) { return Error("mutation queue full, try again after the next step", nullptr); }

    Json::Value r(Json::objectValue);
    r["success"] = true;
    r["queued"] = true;
    Json::Value packs(Json::arrayValue);
    for (const std::string& id : preview.packIds) { packs.append(id); }
    r["packs"] = packs;
    Json::Value warnings(Json::arrayValue);
    for (const std::string& w : preview.warnings) { warnings.append(w); }
    r["warnings"] = warnings;
    const ParamDayInfo day = mDayProvider ? mDayProvider() : ParamDayInfo();
    r["effectiveDay"] = day.effectiveDay;
    if (day.deferLag > 0) { r["visibleDay"] = day.effectiveDay + day.deferLag; }
    return r;
}

bool ParamCommandHelper::ApplyMutation(const ParamMutation& mutation)
{
    if (mutation.kind == ParamMutation::Kind::kReload)
    {
        ParamReloadResult result;
        if (!mReloadFn || !mReloadFn(true, result))
        {
            DIA_LOG_WARNING("WorldSim", "%s: queued reload failed: %s", mName.c_str(), result.error.c_str());
            return false;
        }
        ClearSources();
        for (size_t i = 0; i < result.packIds.size() && i < result.packBodies.size(); ++i)
        {
            NotePackApplied(result.packIds[i].c_str(), result.packBodies[i]);
        }
        return true;
    }
    Dia::Attribute::ParamError err;
    if (!mFile.Set(Dia::Core::StringCRC(mutation.key), mutation.value, err))
    {
        DIA_LOG_WARNING("WorldSim", "%s: queued set of '%s' refused: %s", mName.c_str(), mutation.key, err.reason.AsCStr());
        return false;
    }
    MarkKey(mutation.key, kSourceSet);
    return true;
}

}  // namespace Debug
}  // namespace CoW
