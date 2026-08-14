# System Spec: DiaUtilityAIVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaUtilityAIVisualDebugger is the `IDebugDomain` implementation that makes utility AI score evaluation visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`) — utility scores have no canonical world-space anchor.

The domain card shows per-action utility scores as a sorted list with visual bars, the winning action highlighted, and total action count. All state is surfaced via `GetJSONState()` using `UtilitySet::GetLastFrameScores()`.

**Critical gap:** The live `UtilityScoreDrawer` holds a `const UtilitySet& mUtilitySet` reference and `Draw()` is a no-op. `DrawImGui()` was never implemented (retired interface). `GetJSONState()` has never been wired to call `GetLastFrameScores()`. The score data exists at runtime but nothing emits it to the panel.

## Responsibilities

- Implement `IDebugDomain` — all pure virtual methods
- `HasWorldDrawers()` returns `false` (see SD-001 for optional world-space extension)
- `GetJSONState()` calls `mUtilitySet.GetLastFrameScores(outIds, outScores)`, sorts by descending score, and emits the actions array
- `OnCommand("toggle", {drawer: "Scores"})` — enables/disables the score list section
- `OnCommand("setScale", ...)` — no-op
- Entire module guarded by `#ifdef DIA_DEBUG`

## Non-Responsibilities

- UtilitySet evaluation — `DiaUtilityAI`
- World-space entity position — deferred (SD-001)
- Per-consideration weight breakdown — deferred (Open Design Question 1)
- Any runtime behaviour in Release

## Public Interface

```cpp
// Enhancement to the existing UtilityAIDebugDomain.h / UtilityScoreDrawer.h

class UtilityAIDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit UtilityAIDebugDomain(const UtilitySet& utilitySet);

    Dia::Core::StringCRC  GetDomainId()      const override; // "UtilityAI"
    const char*           GetDisplayName()   const override; // "UtilityAI"
    const char*           GetDescription()   const override; // see below
    Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
    Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

    bool HasWorldDrawers() const override { return false; }

    void GetJSONState(Dia::Core::JsonWriter& writer) override;
    void OnCommand(Dia::Core::StringCRC cmd,
                   const Dia::Core::JsonValue& args) override;

private:
    const UtilitySet& mUtilitySet;
    bool mScoresEnabled = true;
};
```

`GetDescription()` returns: `"Utility AI — per-action scores, winning action, score bars"` (52 chars ≤ 80 ✓)

## JSON State Schema

```json
{
  "drawers": [
    { "name": "Scores", "enabled": true }
  ],
  "stats": {
    "actionCount": 5,
    "winnerScore":  0.83
  },
  "actions": [
    { "id": "attack_nearest",   "score": 0.83, "winner": true  },
    { "id": "seek_health",      "score": 0.61, "winner": false },
    { "id": "patrol_waypoint",  "score": 0.34, "winner": false },
    { "id": "flee_from_danger", "score": 0.22, "winner": false },
    { "id": "idle",             "score": 0.09, "winner": false }
  ]
}
```

`"actions"` sorted by descending score. `"winner"` true for the highest-scored action only. When no `Evaluate()` has run, `"actions"` is an empty array and `"stats.winnerScore"` is 0.

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Spacing:** group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`
- **Stat line:** `"Winner: <action_id>"`
- **Expanded body:** action list sorted by score — each row shows action name + score value + horizontal bar; winner row highlighted in accent color; Drawer toggle "Scores"

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `GetJSONState()` calls `mUtilitySet.GetLastFrameScores(outIds, outScores)` — this is the only data source |
| D-2 | `"actions"` array sorted descending by score before emission |
| D-3 | `"stats.winnerScore"` = score of the first (highest) action; 0.0 when no actions |
| D-4 | Panel action rows display as a bar chart: bar width proportional to score (0–1 → 0–100%) |
| D-5 | Winner action row uses `var(--accent)` background tint |
| D-6 | No `DrawImGui()` call anywhere in this module |
| D-7 | Panel card layout complies with AC-16 spacing contract (see Panel Card Specification above) |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `HasWorldDrawers() = false` | Utility score evaluation has no canonical world anchor. If a world-space best-action label is needed, add `BestActionLabelDrawer` as an opt-in second drawer. Deferred. | Accepted | No |
| SD-002 | Actions sorted descending in `GetJSONState()` | Panel needs consistent order; sort once in C++ rather than requiring JS sort. | Accepted | Yes |
| SD-003 | `GetLastFrameScores()` is the only data source in v1 | Matches the existing `DIA_DEBUG`-guarded API. Per-consideration weights deferred. | Accepted | No |

## Open Design Questions

1. **Consideration weights** — `GetLastFrameScores()` returns action IDs + final scores only. Showing WHY an action scored highly requires `UtilitySet::GetLastConsiderationWeights(actionId, outIds, outWeights)`. Defer to v2 once score bars are validated in the panel.

2. **Normalised vs raw scores** — scores from `GetLastFrameScores()` are in [0,1] (consideration product). Display as-is; no re-normalisation.

## Status

**Status:** Approved
