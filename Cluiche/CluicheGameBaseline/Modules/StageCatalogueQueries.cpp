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
    v["presets"] = Json::Value(Json::arrayValue);
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
    return out;
}

Json::Value Get(const StageCatalogueState& state, const Json::Value& args)
{
    std::string stage;
    if (!StageArg(args, "stage", stage)) { return Error("missing argument 'stage'"); }
    const int i = state.Catalogue().Find(Dia::Core::StringCRC(stage.c_str()));
    if (i < 0) { return Error("unknown stage '" + stage + "'"); }
    return EntryToJson(state, static_cast<unsigned int>(i), true);
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
