#include "StageResult.h"

#include <cstring>

namespace Cluiche { namespace AppFlow {

const char* StageResultSeverityName(StageResultSeverity severity)
{
    switch (severity)
    {
    case StageResultSeverity::kOk:   return "ok";
    case StageResultSeverity::kWarn: return "warn";
    case StageResultSeverity::kFail: return "fail";
    default:                         return "none";
    }
}

bool StageResultSeverityFromName(const char* name, StageResultSeverity& out)
{
    if (name == nullptr) { return false; }
    static const StageResultSeverity all[] = { StageResultSeverity::kNone, StageResultSeverity::kOk, StageResultSeverity::kWarn, StageResultSeverity::kFail };
    for (StageResultSeverity s : all)
    {
        if (std::strcmp(name, StageResultSeverityName(s)) == 0) { out = s; return true; }
    }
    return false;
}

void StageResult::SetLabel(const char* text)
{
    label.Clear();
    if (text != nullptr) { label.AppendAsMuchAsCan(text, kMaxLabel < std::strlen(text) ? kMaxLabel : static_cast<unsigned int>(std::strlen(text))); }
}

void StageResult::SetSessionId(const char* text)
{
    sessionId.Clear();
    if (text != nullptr) { sessionId.AppendAsMuchAsCan(text); }
}

void StageResult::SetLogPath(const char* text)
{
    logPath.Clear();
    if (text != nullptr) { logPath.AppendAsMuchAsCan(text); }
}

bool StageResultsTable::Set(const char* stageName, const StageResult& result)
{
    if (stageName == nullptr || stageName[0] == '\0') { return false; }
    const Dia::Core::StringCRC crc(stageName);
    const int existing = Find(crc);
    if (existing >= 0)
    {
        mEntries[static_cast<unsigned int>(existing)].result = result;
        return true;
    }
    if (mEntries.IsFull()) { return false; }
    Entry e;
    e.name.AppendAsMuchAsCan(stageName);
    e.stage = crc;
    e.result = result;
    mEntries.Add(e);
    return true;
}

bool StageResultsTable::Set(const StageResultEvent& event)
{
    return Set(event.stage.AsChar(), event.result);
}

int StageResultsTable::Find(const Dia::Core::StringCRC& stage) const
{
    for (unsigned int i = 0; i < mEntries.Size(); ++i)
    {
        if (mEntries[i].stage == stage) { return static_cast<int>(i); }
    }
    return -1;
}

int StageResultsTable::Find(const char* stageName) const
{
    return stageName == nullptr ? -1 : Find(Dia::Core::StringCRC(stageName));
}

}} // namespace Cluiche::AppFlow
