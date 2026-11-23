#include <Shared/Debug/ParamCommandHelper.h>

#include <stdio.h>
#include <string.h>

#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {
namespace Debug {

using Dia::Attribute::AttributeKind;

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
    if (action == "set" || action == "reload" || action == "save_defaults")
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
