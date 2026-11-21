#include "Modules/F2PanelModel.h"
#include "Modules/F2TestRunnerModule.h"
#include "Modules/F2Scenario.h"

#include <cstdio>

namespace CoW {

namespace {

constexpr uint8_t kIdlePhase = 0xFF;

const char* const kPhaseShortNames[kF2PanelPhaseCount] = {
    "0 Isolation", "1 Session", "2 Save", "3 Reference", "4 Reload", "5 Replay", "6 Rerun", "7 Fail load" };
const char* const kPhaseLongNames[kF2PanelPhaseCount] = {
    "Isolation", "Session", "Save", "Reference future", "Reload", "Replay", "Rerun", "Failed load" };

const char* const kCheckIds[kF2PanelCheckCount] = { "C1", "C2", "C3", "C4", "C5", "C6" };
const char* const kCheckNames[kF2PanelCheckCount] = {
    "Stream independence", "Determinism", "Save/load RNG round trip", "Clock proof", "Save/load proof", "Band resolver timing" };
// Phase in which each check is raised (C5 resolves in the failed-load phase, C6 at the start of the session).
const int kCheckPhase[kF2PanelCheckCount] = { 0, 6, 5, 1, 7, 1 };

const char* StatusName(uint8_t status)
{
    switch (static_cast<F2CheckStatus>(status))
    {
    case F2CheckStatus::NotRun:  return "NotRun";
    case F2CheckStatus::Running: return "Running";
    case F2CheckStatus::Pass:    return "Pass";
    case F2CheckStatus::Fail:    return "Fail";
    }
    return "NotRun";
}

std::string Format(const char* fmt, unsigned long long a = 0, unsigned long long b = 0)
{
    char buf[160];
    std::snprintf(buf, sizeof(buf), fmt, a, b);
    return buf;
}

std::string FingerprintText(const F2Fingerprint& f)
{
    char buf[160];
    std::snprintf(buf, sizeof(buf), "day %llu | %s | streams captured",
                  static_cast<unsigned long long>(f.dayCounter), SpeedTierName(f.speedTier));
    return buf;
}

// Replay / Rerun row: compared against F1000.
F2PanelFingerprintRow ComparedRow(const char* label, bool have, bool haveReference, bool pending,
                                  const F2Fingerprint& fp, const F2Fingerprint& reference)
{
    F2PanelFingerprintRow row;
    row.label = label;
    if (!have || !haveReference)
    {
        row.state = pending ? "pending" : "none";
        return row;
    }
    const F2CompareResult r = F2Fingerprint::Compare(reference, fp);
    if (r.IsIdentical())
    {
        row.state = "ok";
        row.text = "identical to F1000 (all fields)";
        return row;
    }
    row.state = "diff";
    row.diffFields = r.differingFields;
    for (size_t i = 0; i < r.differingFields.size(); ++i)
    {
        if (i > 0)
            row.text += ", ";
        row.text += r.differingFields[i];
    }
    return row;
}

Json::Value StringArray(const std::vector<std::string>& v)
{
    Json::Value a(Json::arrayValue);
    for (const std::string& s : v)
        a.append(s);
    return a;
}

bool ReadString(const Json::Value& v, const char* key, std::string& out)
{
    if (!v.isObject() || !v.isMember(key) || !v[key].isString())
        return false;
    out = v[key].asString();
    return true;
}

bool ReadBool(const Json::Value& v, const char* key, bool& out)
{
    if (!v.isObject() || !v.isMember(key) || !v[key].isBool())
        return false;
    out = v[key].asBool();
    return true;
}

bool ReadInt(const Json::Value& v, const char* key, int& out)
{
    if (!v.isObject() || !v.isMember(key) || !v[key].isIntegral())
        return false;
    out = v[key].asInt();
    return true;
}

} // namespace

//-----------------------------------------------------------------------------
F2PanelModel F2PanelModel::Build(const F2PanelInput& in)
{
    F2PanelModel m;
    m.running = in.running;

    int resolved = 0;
    for (int i = 0; i < kF2PanelCheckCount; ++i)
    {
        const F2CheckStatus s = static_cast<F2CheckStatus>(in.checkStatus[i]);
        if (s == F2CheckStatus::Pass) { ++m.passCount; ++resolved; }
        else if (s == F2CheckStatus::Fail) { ++m.failCount; ++resolved; }
        else if (s == F2CheckStatus::Running) ++resolved;
    }
    m.complete = !in.running && resolved > 0;

    // ---- Phase strip ----
    const int current = (in.running && in.phase < kF2PanelPhaseCount) ? in.phase : -1;
    for (int i = 0; i < kF2PanelPhaseCount; ++i)
    {
        F2PanelPhase p;
        p.name = kPhaseShortNames[i];
        p.done = m.complete || (current >= 0 && i < current);
        p.current = (i == current);
        m.phases.push_back(p);
    }
    for (int c = 0; c < kF2PanelCheckCount; ++c)
    {
        if (static_cast<F2CheckStatus>(in.checkStatus[c]) != F2CheckStatus::Fail)
            continue;
        F2PanelPhase& p = m.phases[kCheckPhase[c]];
        if (p.done || p.current)
        {
            p.failed = true;
            p.done = false;
        }
    }

    // ---- Tick line ----
    if (in.running)
    {
        if (in.phase < kF2PanelPhaseCount)
        {
            m.tickText = "Phase " + std::string(1, static_cast<char>('0' + in.phase)) + " " + kPhaseLongNames[in.phase];
            const bool ticking = in.phase == static_cast<uint8_t>(F2PhaseId::Session)
                              || in.phase == static_cast<uint8_t>(F2PhaseId::ReferenceFuture)
                              || in.phase == static_cast<uint8_t>(F2PhaseId::Replay)
                              || in.phase == static_cast<uint8_t>(F2PhaseId::Rerun);
            if (ticking)
                m.tickText += Format("  -  tick %llu / %llu", in.scenarioTick, kF2ScenarioTicks);
        }
        else
            m.tickText = "Starting";
    }
    else if (m.complete)
        m.tickText = Format("Scenario complete  -  %llu pass, %llu fail",
                            static_cast<unsigned long long>(m.passCount), static_cast<unsigned long long>(m.failCount));
    else
        m.tickText = "Idle";

    // ---- Check rows ----
    for (int i = 0; i < kF2PanelCheckCount; ++i)
    {
        F2PanelCheckRow row;
        row.id = kCheckIds[i];
        row.name = kCheckNames[i];
        row.status = StatusName(in.checkStatus[i]);
        row.result = in.checkResult[i];
        m.checks.push_back(row);
    }

    // ---- Fingerprints ----
    const bool live = in.running;
    {
        F2PanelFingerprintRow r;
        r.label = "F500";
        if (in.hasF500) { r.state = "ok"; r.text = FingerprintText(in.f500); }
        else r.state = live ? "pending" : "none";
        m.fingerprints.push_back(r);
    }
    {
        F2PanelFingerprintRow r;
        r.label = "F1000";
        if (in.hasF1000) { r.state = "ok"; r.text = FingerprintText(in.f1000); }
        else r.state = live ? "pending" : "none";
        m.fingerprints.push_back(r);
    }
    m.fingerprints.push_back(ComparedRow("Replay F1000", in.hasReplay1000, in.hasF1000, live, in.replay1000, in.f1000));
    m.fingerprints.push_back(ComparedRow("Rerun F1000", in.hasRerun1000, in.hasF1000, live, in.rerun1000, in.f1000));

    // ---- Buttons and labels ----
    m.runEnabled = !in.running;
    m.controlsEnabled = !in.running;
    m.runLabel = in.running ? "Running..." : (m.complete ? "Run Again" : "Run Scenario");

    if (in.hasClock)
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Day %llu | %s | modals %u", static_cast<unsigned long long>(in.day),
                      SpeedTierName(in.speedTier), static_cast<unsigned>(in.modalDepth));
        m.clockText = buf;
        m.speedTier = static_cast<int>(in.speedTier);
    }

    // ---- Footer ----
    if (in.running)
        m.footer = "Scenario running. Manual controls locked.";
    else if (!m.complete)
        m.footer = "Ready.";
    else if (m.failCount == 0)
        m.footer = "All checks passed.";
    else
    {
        m.footer = std::to_string(m.failCount) + (m.failCount == 1 ? " check failed. See " : " checks failed. See ");
        bool first = true;
        for (int i = 0; i < kF2PanelCheckCount; ++i)
        {
            if (static_cast<F2CheckStatus>(in.checkStatus[i]) != F2CheckStatus::Fail)
                continue;
            if (!first)
                m.footer += ", ";
            m.footer += kCheckIds[i];
            first = false;
        }
        m.footer += ".";
    }
    return m;
}

//-----------------------------------------------------------------------------
F2PanelInput F2PanelModel::CaptureInput(const F2TestRunnerModule& runner)
{
    F2PanelInput in;
    in.running = runner.IsRunning();
    in.phase = static_cast<uint8_t>(runner.GetCurrentPhase());
    in.scenarioTick = runner.GetScenarioTick();
    for (int i = 0; i < kF2PanelCheckCount; ++i)
    {
        const F2CheckId id = static_cast<F2CheckId>(i);
        in.checkStatus[i] = static_cast<uint8_t>(runner.GetCheckStatus(id));
        in.checkResult[i] = runner.GetLastResult(id);
    }

    // Which fingerprints exist yet: F500 / F1000 have flags; the replay and rerun captures are
    // valid once their phase has been left (phase order is fixed), or once the run is over.
    const bool over = !in.running && in.phase == kIdlePhase;
    bool anyResolved = false;
    for (int i = 0; i < kF2PanelCheckCount; ++i)
        anyResolved = anyResolved || in.checkStatus[i] != static_cast<uint8_t>(F2CheckStatus::NotRun);
    const bool finished = over && anyResolved;
    in.hasF500 = runner.HasFingerprint500();
    in.hasF1000 = runner.HasFingerprint1000();
    if (in.hasF500) in.f500 = runner.GetFingerprint500();
    if (in.hasF1000) in.f1000 = runner.GetFingerprint1000();
    in.hasReplay1000 = in.hasF1000 && (finished || (in.running && in.phase != kIdlePhase && in.phase > static_cast<uint8_t>(F2PhaseId::Replay)));
    in.hasRerun1000 = in.hasF1000 && (finished || (in.running && in.phase != kIdlePhase && in.phase > static_cast<uint8_t>(F2PhaseId::Rerun)));
    if (in.hasReplay1000) in.replay1000 = runner.GetReplayFingerprint1000();
    if (in.hasRerun1000) in.rerun1000 = runner.GetRerunFingerprint1000();

    GameSpeedState clock;
    if (runner.GetClockState(clock))
    {
        in.hasClock = true;
        in.day = clock.dayCounter;
        in.speedTier = clock.speedTier;
        in.modalDepth = clock.modalDepth;
    }
    return in;
}

//-----------------------------------------------------------------------------
Json::Value F2PanelModel::ToJson() const
{
    Json::Value j(Json::objectValue);

    Json::Value& ph = j["phases"] = Json::Value(Json::arrayValue);
    for (const F2PanelPhase& p : phases)
    {
        Json::Value o(Json::objectValue);
        o["name"] = p.name;
        o["done"] = p.done;
        o["current"] = p.current;
        o["failed"] = p.failed;
        ph.append(o);
    }
    j["tickText"] = tickText;

    Json::Value& ch = j["checks"] = Json::Value(Json::arrayValue);
    for (const F2PanelCheckRow& c : checks)
    {
        Json::Value o(Json::objectValue);
        o["id"] = c.id;
        o["name"] = c.name;
        o["status"] = c.status;
        o["result"] = c.result;
        ch.append(o);
    }

    Json::Value& fp = j["fingerprints"] = Json::Value(Json::arrayValue);
    for (const F2PanelFingerprintRow& r : fingerprints)
    {
        Json::Value o(Json::objectValue);
        o["label"] = r.label;
        o["state"] = r.state;
        o["text"] = r.text;
        o["diffFields"] = StringArray(r.diffFields);
        fp.append(o);
    }

    j["running"] = running;
    j["complete"] = complete;
    j["passCount"] = passCount;
    j["failCount"] = failCount;
    j["runLabel"] = runLabel;
    j["runEnabled"] = runEnabled;
    j["controlsEnabled"] = controlsEnabled;
    j["clockText"] = clockText;
    j["speedTier"] = speedTier;
    j["footer"] = footer;
    return j;
}

//-----------------------------------------------------------------------------
bool F2PanelModel::FromJson(const Json::Value& j, F2PanelModel& out)
{
    if (!j.isObject() || !j["phases"].isArray() || !j["checks"].isArray() || !j["fingerprints"].isArray())
        return false;

    F2PanelModel m;
    for (const Json::Value& o : j["phases"])
    {
        F2PanelPhase p;
        if (!ReadString(o, "name", p.name) || !ReadBool(o, "done", p.done)
            || !ReadBool(o, "current", p.current) || !ReadBool(o, "failed", p.failed))
            return false;
        m.phases.push_back(p);
    }
    for (const Json::Value& o : j["checks"])
    {
        F2PanelCheckRow c;
        if (!ReadString(o, "id", c.id) || !ReadString(o, "name", c.name)
            || !ReadString(o, "status", c.status) || !ReadString(o, "result", c.result))
            return false;
        m.checks.push_back(c);
    }
    for (const Json::Value& o : j["fingerprints"])
    {
        F2PanelFingerprintRow r;
        if (!ReadString(o, "label", r.label) || !ReadString(o, "state", r.state)
            || !ReadString(o, "text", r.text) || !o["diffFields"].isArray())
            return false;
        for (const Json::Value& d : o["diffFields"])
        {
            if (!d.isString())
                return false;
            r.diffFields.push_back(d.asString());
        }
        m.fingerprints.push_back(r);
    }
    if (!ReadString(j, "tickText", m.tickText) || !ReadBool(j, "running", m.running)
        || !ReadBool(j, "complete", m.complete) || !ReadInt(j, "passCount", m.passCount)
        || !ReadInt(j, "failCount", m.failCount) || !ReadString(j, "runLabel", m.runLabel)
        || !ReadBool(j, "runEnabled", m.runEnabled) || !ReadBool(j, "controlsEnabled", m.controlsEnabled)
        || !ReadString(j, "clockText", m.clockText) || !ReadInt(j, "speedTier", m.speedTier)
        || !ReadString(j, "footer", m.footer))
        return false;

    out = m;
    return true;
}

} // namespace CoW
