#pragma once

#include <Modules/StageCatalogueLoader.h>
#include <DiaCore/Json/external/json/json.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Test helpers for the stage catalogue: fixture lookup and rendering a catalogue to the
// expected_catalogue.json shape (same shape the Python loader emits).
namespace Cluiche { namespace AppFlow { namespace Testing {

// Walks up from `sourceFile` (pass __FILE__) to find Cluiche/Tests/Fixtures/StageCatalogue.
inline std::filesystem::path FindStageCatalogueFixtures(const char* sourceFile)
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(sourceFile), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue";
        if (std::filesystem::exists(candidate, ec)) { return candidate; }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) { break; }
        dir = parent;
    }
    return std::filesystem::path();
}

inline bool ReadJsonFile(const std::filesystem::path& path, Json::Value& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    Json::Reader reader;
    return reader.parse(ss.str(), out);
}

inline Json::Value KindNames(Dia::Game::StageKindMask mask)
{
    Json::Value arr(Json::arrayValue);
    for (unsigned int i = 0; i < Dia::Game::kStageKindCount; ++i)
    {
        if (Dia::Game::MaskHasKind(mask, static_cast<Dia::Game::StageKind>(i))) { arr.append(Dia::Game::kStageKindNames[i]); }
    }
    return arr;
}

// Mirrors to_expected_json in stage_catalogue.py.
inline Json::Value ToExpectedJson(StageCatalogueResult result, const StageCatalogue& cat, bool summaryOnly = false)
{
    Json::Value out(Json::objectValue);
    out["result"] = StageCatalogueResultName(result);
    if (result != StageCatalogueResult::kOk) { return out; }
    if (summaryOnly)
    {
        out["entryCount"] = static_cast<int>(cat.Size());
        out["droppedCount"] = static_cast<int>(cat.DroppedCount());
        out["omittedCount"] = static_cast<int>(cat.OmittedCount());
        return out;
    }
    Json::Value entries(Json::arrayValue);
    for (unsigned int i = 0; i < cat.Size(); ++i)
    {
        const StageCatalogueEntry& e = cat[i];
        Json::Value j(Json::objectValue);
        j["name"] = e.name.AsChar();
        Json::Value tags(Json::arrayValue);
        for (unsigned int t = 0; t < e.tags.Size(); ++t) { tags.append(e.tags[t].AsChar()); }
        j["tags"] = tags;
        j["kinds"] = KindNames(e.kinds);
        j["description"] = e.description.AsCStr();
        entries.append(j);
    }
    out["entries"] = entries;
    StageCatalogueRows rows;
    cat.GroupByKind(rows);
    Json::Value jr(Json::arrayValue);
    for (unsigned int i = 0; i < rows.Size(); ++i)
    {
        Json::Value r(Json::objectValue);
        r["group"] = StageGroupName(rows[i].group);
        r["entry"] = static_cast<int>(rows[i].entryIndex);
        jr.append(r);
    }
    out["rows"] = jr;
    out["omittedCount"] = static_cast<int>(cat.OmittedCount());
    out["droppedCount"] = static_cast<int>(cat.DroppedCount());
    return out;
}

}}} // namespace Cluiche::AppFlow::Testing
