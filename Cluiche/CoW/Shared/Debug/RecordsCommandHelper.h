#pragma once

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include <DiaCore/Json/external/json/json.h>
#include <Shared/Debug/SimCommandSet.h>
#include <Shared/World/WorldSimNames.h>

namespace CoW {
namespace Debug {

// Where a bad record is: file, record id and field. Any part may be empty when unknown.
struct RecordError
{
    std::string file;
    std::string record;
    std::string field;
    std::string text;
};

// What the system's reload callback reports. On failure (ok == false) the callback has kept the previous records.
struct RecordReloadResult
{
    bool ok = true;
    RecordError error;
    std::vector<std::string> ids;     // the new record ids on success
};

// One sim-hosted command cow.<sys>.records serving a system's record files (species, flow graph, succession rules, tokens).
// Actions: list (current ids), reload (queued; applied by the owner at the next Capture through ApplyReload).
// Replies use the SimQueryHost shape: {"success":true,...} or {"success":false,"error":"..."}. SimPU only.
class RecordsCommandHelper
{
public:
    typedef std::function<RecordReloadResult()> RecordReloadFn;
    typedef std::function<std::vector<std::string>()> RecordListFn;
    // Queues one reload for the owner's next Capture; false when the queue is full.
    typedef std::function<bool()> ReloadEnqueue;
    // failing=true with text naming file, record and field; failing=false clears it (text empty).
    typedef std::function<void(bool failing, const std::string& text)> HealthFn;

    explicit RecordsCommandHelper(const char* sysToken);

    // cow.<sys>.records; empty for an unknown token.
    const char* CommandName() const { return mName.c_str(); }

    // Registration entry (mandatory help). The helper must outlive the registration.
    SimCommandEntry BuildEntry();
    Json::Value Handle(const Json::Value& args);

    void SetReloadFn(RecordReloadFn fn) { mReloadFn = std::move(fn); }
    void SetListFn(RecordListFn fn) { mListFn = std::move(fn); }
    void SetReloadEnqueue(ReloadEnqueue fn) { mEnqueue = std::move(fn); }
    void SetHealthFn(HealthFn fn) { mHealth = std::move(fn); }

    // Owner's ApplyEntry (inside Capture) for a drained reload. Runs the reload fn; failure reports Failing naming
    // file, record and field (previous records stay), success clears it. Returns the reload result's ok.
    bool ApplyReload();

private:
    static Json::Value Error(const char* text);

    World::Name mName;
    RecordReloadFn mReloadFn;
    RecordListFn mListFn;
    ReloadEnqueue mEnqueue;
    HealthFn mHealth;
};

}  // namespace Debug
}  // namespace CoW
