#include "Modules/StageCatalogueQueries.h"

#include <Modules/StageCatalogueState.h>

#include <cstring>
#include <string>

namespace Cluiche { namespace AppFlow { namespace StageCatalogueQueries {

namespace {

Json::Value Error(const std::string& message)
{
    Json::Value v(Json::objectValue);
    v["error"] = message;
    return v;
}

Json::Value ResultToJson(const StageResult& r)
{
    Json::Value v(Json::objectValue);
    v["severity"] = StageResultSeverityName(r.severity);
    v["label"] = r.label.AsCStr();
    v["utc"] = static_cast<Json::UInt64>(r.utcMs);
    v["session_id"] = r.sessionId.AsCStr();
    v["log_path"] = r.logPath.AsCStr();
    return v;
}

Json::Value EntryToJson(const StageCatalogueState& state, unsigned int index, bool full)
{
    const StageCatalogueEntry& e = state.Catalogue()[index];
    Json::Value v(Json::objectValue);
    v["name"] = e.name.AsChar();
    Json::Value kinds(Json::arrayValue);
    for (unsigned int k = 0; k < Dia::Game::kStageKindCount; ++k)
    {
        if (Dia::Game::MaskHasKind(e.kinds, static_cast<Dia::Game::StageKind>(k))) { kinds.append(Dia::Game::kStageKindNames[k]); }
    }
    v["kinds"] = kinds;
    Json::Value tags(Json::arrayValue);
    for (unsigned int t = 0; t < e.tags.Size(); ++t) { tags.append(e.tags[t].AsChar()); }
    v["tags"] = tags;
    v["description"] = e.description.AsCStr();
    Json::Value presets(Json::arrayValue);
    for (unsigned int p = 0; p < e.presets.Size(); ++p)
    {
        Json::Value preset(Json::objectValue);
        preset["name"] = e.presets[p].name.AsCStr();
        preset["description"] = e.presets[p].description.AsCStr();
        presets.append(preset);
    }
    v["presets"] = presets;
    const int r = state.Results().Find(e.name);
    v["last_result"] = r >= 0 ? ResultToJson(state.Results()[static_cast<unsigned int>(r)].result) : Json::Value(Json::nullValue);
    if (full) { v["proves"] = Json::Value(Json::arrayValue); }
    return v;
}

bool IsKindName(const std::string& s)
{
    if (s == "other") { return true; }
    for (unsigned int k = 0; k < Dia::Game::kStageKindCount; ++k)
    {
        if (s == Dia::Game::kStageKindNames[k]) { return true; }
    }
    return false;
}

bool HasKind(const StageCatalogueEntry& e, const std::string& kind)
{
    if (kind == "other") { return e.kinds == 0; }
    for (unsigned int k = 0; k < Dia::Game::kStageKindCount; ++k)
    {
        if (kind == Dia::Game::kStageKindNames[k]) { return Dia::Game::MaskHasKind(e.kinds, static_cast<Dia::Game::StageKind>(k)); }
    }
    return false;
}

bool StageArg(const Json::Value& args, const char* key, std::string& out)
{
    if (!args.isObject() || !args.isMember(key) || !args[key].isString()) { return false; }
    out = args[key].asString();
    return true;
}

} // namespace

Json::Value List(const StageCatalogueState& state, const Json::Value& args)
{
    Dia::Core::StringCRC tags[StageCatalogueEntry::kMaxTags];
    unsigned int tagCount = 0;
    if (args.isObject() && args.isMember("tag"))
    {
        const Json::Value& t = args["tag"];
        if (t.isString())
        {
            tags[tagCount++] = Dia::Core::StringCRC(t.asCString());
        }
        else if (t.isArray())
        {
            for (Json::ArrayIndex i = 0; i < t.size(); ++i)
            {
                if (!t[i].isString()) { return Error("tag must be an array of strings"); }
                if (tagCount >= StageCatalogueEntry::kMaxTags) { return Error("too many tags"); }
                tags[tagCount++] = Dia::Core::StringCRC(t[i].asCString());
            }
        }
        else if (!t.isNull()) { return Error("tag must be an array of strings"); }
    }

    std::string kind;
    const bool hasKind = StageArg(args, "kind", kind);
    if (hasKind && !IsKindName(kind)) { return Error("unknown kind '" + kind + "'"); }

    StageCatalogueIndices indices;
    state.Catalogue().Filter(tags, tagCount, indices);
    Json::Value out(Json::arrayValue);
    for (unsigned int i = 0; i < indices.Size(); ++i)
    {
        if (hasKind && !HasKind(state.Catalogue()[indices[i]], kind)) { continue; }
        out.append(EntryToJson(state, indices[i], false));
    }
    // Wrapped: the DebugServer wire payload is a proto Struct, which drops array roots.
    Json::Value reply(Json::objectValue);
    reply["stages"] = out;
    return reply;
}

Json::Value Get(const StageCatalogueState& state, const Json::Value& args)
{
    std::string stage;
    if (!StageArg(args, "stage", stage)) { return Error("missing argument 'stage'"); }
    const int i = state.Catalogue().Find(Dia::Core::StringCRC(stage.c_str()));
    if (i < 0) { return Error("unknown stage '" + stage + "'"); }
    return EntryToJson(state, static_cast<unsigned int>(i), true);
}

bool ValidateLaunch(const StageCatalogueState& state, const Json::Value& args, bool atBoot, StageLaunchRequest& out, std::string& error)
{
    std::string stage;
    if (!StageArg(args, "stage", stage)) { error = "missing argument 'stage'"; return false; }
    if (!atBoot) { error = "stages.launch is only available at the Boot stage"; return false; }
    const int i = state.Catalogue().Find(Dia::Core::StringCRC(stage.c_str()));
    if (i < 0) { error = "unknown stage '" + stage + "'"; return false; }

    std::string preset;
    if (args.isMember("preset") && !args["preset"].isNull())
    {
        if (!args["preset"].isString()) { error = "preset must be a string"; return false; }
        preset = args["preset"].asString();
    }
    if (!preset.empty())
    {
        const StageCatalogueEntry& e = state.Catalogue()[static_cast<unsigned int>(i)];
        bool found = false;
        for (unsigned int p = 0; p < e.presets.Size() && !found; ++p) { found = preset == e.presets[p].name.AsCStr(); }
        if (!found) { error = "unknown preset '" + preset + "' for stage '" + stage + "'"; return false; }
    }
    out.stage = Dia::Core::StringCRC(stage.c_str());
    out.preset = preset.empty() ? Dia::Core::StringCRC() : Dia::Core::StringCRC(preset.c_str());
    return true;
}

Json::Value Results(const StageCatalogueState& state, const Json::Value& args)
{
    std::string stage;
    if (StageArg(args, "stage", stage))
    {
        if (state.Catalogue().Find(Dia::Core::StringCRC(stage.c_str())) < 0) { return Error("unknown stage '" + stage + "'"); }
        const int r = state.Results().Find(Dia::Core::StringCRC(stage.c_str()));
        return r >= 0 ? ResultToJson(state.Results()[static_cast<unsigned int>(r)].result) : Json::Value(Json::nullValue);
    }
    Json::Value out(Json::objectValue);
    for (unsigned int i = 0; i < state.Results().Count(); ++i)
    {
        const StageResultsTable::Entry& e = state.Results()[i];
        out[e.name.AsCStr()] = ResultToJson(e.result);
    }
    return out;
}

}}} // namespace Cluiche::AppFlow::StageCatalogueQueries
