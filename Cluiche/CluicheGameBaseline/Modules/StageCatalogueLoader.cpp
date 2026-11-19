#include "StageCatalogueLoader.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaGame/JsonDiaStageSerializer.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace Cluiche { namespace AppFlow {

namespace {

namespace fs = std::filesystem;
const char* const kBootStage = "Boot";

bool ReadJson(const fs::path& path, Json::Value& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    Json::Reader reader;
    return reader.parse(ss.str(), out);
}

// Import paths are written for the deployed layout; in a source tree the file may sit elsewhere, so fall back to a
// unique file-name match under base (same rule as the Python loader).
fs::path Resolve(const fs::path& base, const std::string& rel)
{
    std::error_code ec;
    const fs::path direct = base / fs::path(rel);
    if (fs::exists(direct, ec)) { return direct; }
    const std::string fileName = fs::path(rel).filename().string();
    fs::path match;
    unsigned int count = 0;
    for (fs::recursive_directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec))
    {
        if (it->is_regular_file(ec) && it->path().filename().string() == fileName)
        {
            match = it->path();
            ++count;
        }
    }
    return count == 1 ? match : direct;
}

StageCatalogueEntry MakeEntry(const char* name)
{
    StageCatalogueEntry e;
    e.name = Dia::Core::StringCRC(name);
    return e;
}

} // namespace

const char* StageCatalogueResultName(StageCatalogueResult result)
{
    switch (result)
    {
    case StageCatalogueResult::kOk:             return "Ok";
    case StageCatalogueResult::kMissingDiagame: return "MissingDiagame";
    case StageCatalogueResult::kMalformed:      return "Malformed";
    default:                                    return "NoRootManifest";
    }
}

StageCatalogueResult StageCatalogueLoader::Load(const char* diagamePath, StageCatalogue& out)
{
    out.Clear();
    if (diagamePath == nullptr) { return StageCatalogueResult::kMissingDiagame; }

    std::error_code ec;
    const fs::path gamePath(diagamePath);
    if (!fs::is_regular_file(gamePath, ec)) { return StageCatalogueResult::kMissingDiagame; }

    Json::Value game;
    if (!ReadJson(gamePath, game) || !game.isObject()) { return StageCatalogueResult::kMalformed; }

    const fs::path base = gamePath.parent_path();
    const Json::Value& imports = game["imports"];
    if (!imports.isNull() && !imports.isArray()) { return StageCatalogueResult::kMalformed; }

    // Root manifest: first import of type "manifest".
    const Json::Value* manifestImport = nullptr;
    for (Json::ArrayIndex i = 0; i < imports.size(); ++i)
    {
        const Json::Value& imp = imports[i];
        if (imp.isObject() && imp["type"].isString() && imp["type"].asString() == "manifest")
        {
            manifestImport = &imp;
            break;
        }
    }
    if (manifestImport == nullptr) { return StageCatalogueResult::kNoRootManifest; }
    if (!(*manifestImport)["path"].isString()) { return StageCatalogueResult::kMalformed; }

    Json::Value app;
    if (!ReadJson(Resolve(base, (*manifestImport)["path"].asString()), app) || !app.isObject()) { return StageCatalogueResult::kMalformed; }

    std::vector<std::string> transitions;
    const Json::Value& stages = app["stages"];
    for (Json::ArrayIndex i = 0; i < stages.size(); ++i)
    {
        const Json::Value& s = stages[i];
        if (!s.isObject() || !s["name"].isString() || s["name"].asString() != kBootStage) { continue; }
        const Json::Value& trans = s["transitions"];
        for (Json::ArrayIndex t = 0; t < trans.size(); ++t)
        {
            if (!trans[t].isString()) { return StageCatalogueResult::kMalformed; }
            transitions.push_back(trans[t].asString());
        }
        break;
    }

    // Imported .diastage files, keyed by the name inside the file.
    std::map<std::string, std::unique_ptr<StageCatalogueEntry>> byName;
    for (Json::ArrayIndex i = 0; i < imports.size(); ++i)
    {
        const Json::Value& imp = imports[i];
        if (!imp.isObject() || !imp["type"].isString() || imp["type"].asString() != "stage") { continue; }
        if (!imp["path"].isString()) { return StageCatalogueResult::kMalformed; }

        const fs::path stagePath = Resolve(base, imp["path"].asString());
        Dia::Game::DiaStageManifest stage;
        Dia::Game::JsonDiaStageSerializer serializer;
        if (!serializer.LoadFromFile(stagePath.string().c_str(), stage)) { return StageCatalogueResult::kMalformed; }

        auto entry = std::make_unique<StageCatalogueEntry>(MakeEntry(stage.name.AsCStr()));
        for (unsigned int t = 0; t < stage.tags.Size(); ++t)
        {
            const Dia::Core::StringCRC tag(stage.tags[t].AsCStr());
            entry->tags.Add(tag);
            Dia::Game::StageKind kind;
            if (Dia::Game::KindFromTag(tag, kind)) { entry->kinds |= Dia::Game::KindBit(kind); }
        }
        entry->description.AppendAsMuchAsCan(stage.description.AsCStr());
        entry->diastagePath.AppendAsMuchAsCan(stagePath.generic_string().c_str());
        byName[stage.name.AsCStr()] = std::move(entry);
    }

    unsigned int dropped = 0;
    for (const std::string& name : transitions)
    {
        const auto it = byName.find(name);
        const StageCatalogueEntry entry = (it != byName.end()) ? *it->second : MakeEntry(name.c_str());
        if (!out.Add(entry)) { ++dropped; }
    }

    const std::set<std::string> listed(transitions.begin(), transitions.end());
    unsigned int omitted = 0;
    for (const auto& kv : byName)
    {
        if (listed.find(kv.first) == listed.end()) { ++omitted; }
    }
    out.SetDroppedCount(dropped);
    out.SetOmittedCount(omitted);
    return StageCatalogueResult::kOk;
}

}} // namespace Cluiche::AppFlow
