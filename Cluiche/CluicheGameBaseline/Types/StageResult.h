#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String256.h>

#include <cstdint>

namespace Cluiche { namespace AppFlow {

enum class StageResultSeverity : uint8_t { kNone, kOk, kWarn, kFail };
// File strings: "none", "ok", "warn", "fail".
const char* StageResultSeverityName(StageResultSeverity severity);
// False (out untouched) for an unknown name.
bool StageResultSeverityFromName(const char* name, StageResultSeverity& out);

struct StageResult {
    static constexpr unsigned int kMaxLabel = 16;  // label is truncated to this many characters

    StageResultSeverity severity = StageResultSeverity::kNone;
    Dia::Core::Containers::String32 label;  // at most kMaxLabel characters (see SetLabel)
    uint64_t utcMs = 0;
    Dia::Core::Containers::String64 sessionId;
    Dia::Core::Containers::String256 logPath;

    void SetLabel(const char* text);
    void SetSessionId(const char* text);
    void SetLogPath(const char* text);
};

struct StageResultEvent {
    Dia::Core::StringCRC stage;
    StageResult result;
};

// Results keyed by stage name (the results file is keyed by name text; lookups use the CRC, PD-001).
class StageResultsTable {
public:
    static constexpr unsigned int kMaxStages = 64;

    struct Entry {
        Dia::Core::Containers::String64 name;  // display name, the file key
        Dia::Core::StringCRC stage;
        StageResult result;
    };

    void Clear() { mEntries.RemoveAll(); }
    unsigned int Count() const { return mEntries.Size(); }
    const Entry& operator[](unsigned int index) const { return mEntries[index]; }

    // Replaces an existing stage's result. False (not stored) when a new stage would exceed kMaxStages or name is null/empty.
    bool Set(const char* stageName, const StageResult& result);
    bool Set(const StageResultEvent& event);
    // Index of the stage, or -1.
    int Find(const Dia::Core::StringCRC& stage) const;
    int Find(const char* stageName) const;

private:
    Dia::Core::Containers::DynamicArrayC<Entry, kMaxStages> mEntries;
};

}} // namespace Cluiche::AppFlow
