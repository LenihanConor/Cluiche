#include "StageResultsFile.h"

#include <DiaCore/Json/external/json/json.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Cluiche { namespace AppFlow {

namespace {

namespace fs = std::filesystem;

// Replaces `to` with `from` (atomic on Windows via MoveFileExW).
bool ReplaceFile(const fs::path& from, const fs::path& to)
{
#ifdef _WIN32
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    fs::rename(from, to, ec);
    return !ec;
#endif
}

void ReadText(const fs::path& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
}

} // namespace

const char* StageResultsReadResultName(StageResultsReadResult result)
{
    switch (result)
    {
    case StageResultsReadResult::kOk:          return "Ok";
    case StageResultsReadResult::kMissing:     return "Missing";
    case StageResultsReadResult::kMalformed:   return "Malformed";
    case StageResultsReadResult::kWrongFormat: return "WrongFormat";
    default:                                   return "UnsupportedVersion";
    }
}

StageResultsReadResult StageResultsFile::Read(const char* path, StageResultsTable& table)
{
    table.Clear();
    if (path == nullptr) { return StageResultsReadResult::kMissing; }

    std::error_code ec;
    const fs::path p(path);
    if (!fs::is_regular_file(p, ec)) { return StageResultsReadResult::kMissing; }

    std::string text;
    ReadText(p, text);
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(text, root) || !root.isObject()) { return StageResultsReadResult::kMalformed; }

    if (!root["format"].isString() || root["format"].asString() != kFormat) { return StageResultsReadResult::kWrongFormat; }
    if (!root["version"].isIntegral() || root["version"].asInt() != kVersion) { return StageResultsReadResult::kUnsupportedVersion; }

    const Json::Value& results = root["results"];
    if (!results.isObject()) { return StageResultsReadResult::kMalformed; }

    const Json::Value::Members names = results.getMemberNames();
    for (const std::string& name : names)
    {
        const Json::Value& e = results[name];
        if (!e.isObject()) { continue; }
        StageResult r;
        if (e["severity"].isString()) { StageResultSeverityFromName(e["severity"].asCString(), r.severity); }
        if (e["label"].isString()) { r.SetLabel(e["label"].asCString()); }
        if (e["utc"].isIntegral()) { r.utcMs = e["utc"].asUInt64(); }
        if (e["session_id"].isString()) { r.SetSessionId(e["session_id"].asCString()); }
        if (e["log_path"].isString()) { r.SetLogPath(e["log_path"].asCString()); }
        table.Set(name.c_str(), r);
    }
    return StageResultsReadResult::kOk;
}

bool StageResultsFile::Write(const char* path, const StageResultsTable& table)
{
    if (path == nullptr || path[0] == '\0') { return false; }

    // Merge with whatever is on disk (invalid/missing disk file reads as empty).
    auto merged = std::make_unique<StageResultsTable>();
    Read(path, *merged);
    for (unsigned int i = 0; i < table.Count(); ++i)
    {
        const StageResultsTable::Entry& e = table[i];
        const int onDisk = merged->Find(e.stage);
        if (onDisk >= 0 && (*merged)[static_cast<unsigned int>(onDisk)].result.utcMs > e.result.utcMs) { continue; }
        merged->Set(e.name.AsCStr(), e.result);
    }

    Json::Value root(Json::objectValue);
    root["format"] = kFormat;
    root["version"] = kVersion;
    Json::Value results(Json::objectValue);
    for (unsigned int i = 0; i < merged->Count(); ++i)
    {
        const StageResultsTable::Entry& e = (*merged)[i];
        Json::Value v(Json::objectValue);
        v["severity"] = StageResultSeverityName(e.result.severity);
        v["label"] = e.result.label.AsCStr();
        v["utc"] = static_cast<Json::UInt64>(e.result.utcMs);
        v["session_id"] = e.result.sessionId.AsCStr();
        v["log_path"] = e.result.logPath.AsCStr();
        results[e.name.AsCStr()] = v;
    }
    root["results"] = results;

    std::error_code ec;
    const fs::path target(path);
    if (target.has_parent_path())
    {
        fs::create_directories(target.parent_path(), ec);
        if (ec) { return false; }
    }

    const fs::path tmp = fs::path(std::string(path) + ".tmp");
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { return false; }
        f << Json::StyledWriter().write(root);
        f.flush();
        if (!f) { f.close(); fs::remove(tmp, ec); return false; }
    }
    if (!ReplaceFile(tmp, target))
    {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

}} // namespace Cluiche::AppFlow
