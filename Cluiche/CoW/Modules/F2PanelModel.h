// F2PanelModel: pure view model for the F-2 TestRunnerPanel. Turns a snapshot of the runner's
// status (F2PanelInput) into everything the panel shows: phase strip flags, tick text, six check
// rows, fingerprint rows with differing fields, button enable state, labels and the footer line.
// No UI, bus or module dependencies in Build(); the panel only renders this and posts requests.
// Spec: docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md
// Mockup: docs/mockups/f2-core-runtime-panel-mockup.html
#pragma once

#include "Modules/F2Fingerprint.h"
#include <Shared/GameSpeedController.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstdint>
#include <string>
#include <vector>

namespace CoW {

class F2TestRunnerModule;

constexpr int kF2PanelPhaseCount = 8;
constexpr int kF2PanelCheckCount = 6;

// Snapshot of runner state. Plain data so the model is testable without a running application.
// phase / status use the F2PhaseId / F2CheckStatus numeric values (phase 0xFF = idle).
struct F2PanelInput
{
    bool     running = false;
    uint8_t  phase = 0xFF;
    uint32_t scenarioTick = 0;
    uint8_t  checkStatus[kF2PanelCheckCount] = {};   // F2CheckStatus values
    std::string checkResult[kF2PanelCheckCount];

    bool hasF500 = false;
    bool hasF1000 = false;
    bool hasReplay1000 = false;
    bool hasRerun1000 = false;
    F2Fingerprint f500;
    F2Fingerprint f1000;
    F2Fingerprint replay1000;
    F2Fingerprint rerun1000;

    bool        hasClock = false;
    uint64_t    day = 0;
    SpeedTier   speedTier = SpeedTier::Paused;
    uint8_t     modalDepth = 0;
};

struct F2PanelPhase
{
    std::string name;
    bool done = false;
    bool current = false;
    bool failed = false;
};

struct F2PanelCheckRow
{
    std::string id;       // "C1".."C6"
    std::string name;
    std::string status;   // "NotRun" | "Running" | "Pass" | "Fail"
    std::string result;
};

struct F2PanelFingerprintRow
{
    std::string label;                      // "F500", "F1000", "Replay F1000", "Rerun F1000"
    std::string state;                      // "none" | "pending" | "ok" | "diff"
    std::string text;
    std::vector<std::string> diffFields;    // non-empty only when state == "diff"
};

struct F2PanelModel
{
    std::vector<F2PanelPhase> phases;
    std::string tickText;
    std::vector<F2PanelCheckRow> checks;
    std::vector<F2PanelFingerprintRow> fingerprints;

    bool        running = false;
    bool        complete = false;       // a run has finished (not running, at least one check resolved)
    int         passCount = 0;
    int         failCount = 0;
    std::string runLabel;               // "Run Scenario" | "Running..." | "Run Again"
    bool        runEnabled = true;
    bool        controlsEnabled = true; // manual speed / step / modal buttons
    std::string clockText;              // "Day N | Tier | modals N"
    int         speedTier = 0;          // selected speed button (SpeedTier value)
    std::string footer;

    static F2PanelModel Build(const F2PanelInput& input);
    // Reads the runner (and, through it, the game clock) into an input snapshot.
    static F2PanelInput CaptureInput(const F2TestRunnerModule& runner);

    Json::Value ToJson() const;
    // Returns false (leaving `out` untouched) when `json` is not a model written by ToJson().
    static bool FromJson(const Json::Value& json, F2PanelModel& out);
};

} // namespace CoW
