# Feature Spec: Panel Palette

**Parent:** @docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
**Mockup:** @docs/specs/applications/cow/systems/f1-harness/mockups/debug-palette.html (task 1 updates it: Ctrl+P hint, Commands and Domains tabs only)
**Status:** Approved
**Plan:** @docs/specs/applications/dia/systems/diadebugdomain/panel-palette.plan.md
**Depends on:** @docs/specs/applications/dia/systems/diadebugdomain/panel-console.md (`FuzzyMatcher`, console strip, thread-safe command list)

## Summary

A searchable overlay inside DiaDebugPanel, opened with **Ctrl+P**, that finds every registered DiaAPI command and every debug domain. Selecting a command puts it in the console strip ready to run; selecting a domain reveals its card in the panel. It is an overlay in `debug-panel.html`, not a separate module or page. The earlier draft's "Panels" tab is gone (DiaDebugDataPanel was dropped; panels are domains).

## Problem

Commands are registered across several systems and domains across 20+ visual debugger modules. A developer has to remember exact names (`debug.layer.enable`? `debug.layers.enable`?) or read source. The data to answer "what exists?" is already in the process (`ListCommands()`, `HelpSystem`, `DiaDebugDomainRegistry`) but nothing presents it.

## Goals

1. Ctrl+P opens the palette while the panel is visible; Esc closes it.
2. One search box matches commands (name, description, category) and domains (name, group).
3. Fuzzy ranking: exact prefix > contains > subsequence, ties alphabetical (same `FuzzyMatcher` as console autocomplete).
4. Tabs: All / Commands / Domains with result counts.
5. Up/Down/Enter keyboard navigation.
6. Enter on a command closes the palette and puts the command text, plus its `example` usage when present, into the console strip with focus. It does not run it.
7. Enter on a domain closes the palette, expands the domain's group accordion and scrolls to the card.
8. The index is rebuilt each time the palette opens, so commands registered or unregistered since last open are reflected.

## Non-Goals

- A separate module, window or global key. Works only while the panel is open.
- Panels tab (dropped with DiaDebugDataPanel).
- Recent or favourite items, keybind display or editing (no command keybinds exist).
- Running a command straight from the palette.
- Opening the panel from Ctrl+P when it is hidden (see Open Design Questions).

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `PaletteIndex::Build` lists every command from the registry and every domain from the domain registry, each with type, name, description and category/group | GoogleTest |
| AC-2 | `PaletteIndex::Search("")` returns everything; `Search("zzzz")` returns nothing | GoogleTest |
| AC-3 | Ranking order is prefix, then contains, then subsequence, ties alphabetical; `"bstrfc"` finds `bus_traffic.start` | GoogleTest |
| AC-4 | Type filter (Commands / Domains) restricts results without re-ranking | GoogleTest |
| AC-5 | A command unregistered between two `Build` calls is absent from the second index | GoogleTest |
| AC-6 | Results are capped at 50 per search and the total match count is reported | GoogleTest |
| AC-7 | `PaletteExecutor::Resolve` returns "fill console with `<text>`" for a command and "reveal domain `<id>`" for a domain | GoogleTest |
| AC-8 | Ctrl+P in the panel opens the overlay with the search box focused; Esc closes it | Manual: `dia run cluichetest` |
| AC-9 | Typing `layer` shows `debug.layer.*` commands and layer-related domains | Manual |
| AC-10 | Enter on a command fills the console strip and focuses it; Enter again runs it (via panel-console) | Manual |
| AC-11 | Enter on a domain expands its group and scrolls to the card | Manual |
| AC-12 | Same behaviour in CoW after the CoW wiring task | Manual |
| AC-13 | Overlay matches the updated mockup | Visual comparison |

## Design

### Where things live

```
Dia/DiaVisualDebugger/Console/        (same folder as panel-console.md)
  FuzzyMatcher.h/.cpp                 (from panel-console)
  PaletteIndex.h/.cpp                 Build() from registries, Search(query, type, out)
  PaletteExecutor.h/.cpp              Resolve(item) -> action (pure, no UI)

Cluiche/CluicheGameBaseline/Modules/
  DebugPanelPageModule.*              owns PaletteIndex; bindings onPaletteOpen / onPaletteSearch
  DebugPanelPage.*                    JS bindings

Cluiche/Assets/CluicheTest/Global/Presentation/UI/debug-panel.html   overlay markup + keys
```

### Types

```cpp
namespace Dia::VisualDebugger::Console {

enum class PaletteItemType { Command, Domain };

struct PaletteItem {
    PaletteItemType type;
    char name[64];
    char description[128];
    char category[32];     // command category, or domain group
    char example[128];     // commands only, may be empty
    char domainId[64];     // domains only
};

class PaletteIndex {
public:
    void Build(const Dia::VisualDebugger::DiaDebugDomainRegistry& domains);
    // Fills out with indices into items, ranked; returns total matches.
    int Search(const char* query, int typeFilterMask,
               Dia::Core::Containers::DynamicArrayC<int, 50>& out) const;
    const PaletteItem& GetItem(int index) const;
    int GetItemCount() const;
};

struct PaletteAction { enum Kind { FillConsole, RevealDomain } kind; char text[160]; };
// RevealDomain.text is the panel card key = the domain's DISPLAY NAME (DebugPanelPageModule roots the
// panel JSON at GetDisplayName(), and the page uses it as data-domain). GetDomainId() can differ
// (MessageBus vs "Message Bus"), so it is not used for reveal. Found 2026-10-13 in palette task 4 review.
PaletteAction ResolvePaletteAction(const PaletteItem& item);

}
```

`Build` calls `ListCommandInfos` (copies, from command-unregister.md), so it is safe on the Main thread while the Sim thread registers or unregisters.

### Flow

```
Ctrl+P (JS keydown) -> app.onPaletteOpen()        C++: PaletteIndex::Build, push item count
type "lay"          -> app.onPaletteSearch("lay", mask)   (query <= 60 chars, fits the 63-char bridge)
                    <- CallJSFunction("paletteResults", json)   top 50 + total
Enter on item       -> JS acts locally: FillConsole sets the strip's input; RevealDomain opens the card
```

`onPaletteOpen` and `onPaletteSearch` only record the request; `DebugPanelPageModule::ProcessDeferredRequests()` builds the index and pushes `paletteResults` from `DoUpdate` (next frame). Doing it inside the callback re-entered the JS engine and crashed the app on Ctrl+P (found 2026-10-13). Latest search wins; an open request discards an older pending search.

Selecting needs no C++ round trip because the result JSON already carries the text to fill or the domain id to reveal; `ResolvePaletteAction` is the tested definition of that mapping and the JS mirrors it (AC-7 locks the contract).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| AD-001 (f1-harness) | Baseline, Dia and CluicheTest edits need explicit human sign-off per change | Plan marks baseline and asset tasks as sign-off gated |
| (Dia rule) | No new singletons | Index is owned by `DebugPanelPageModule` |
| SD-F1-001 | Debug-only | Baseline wiring inside `#ifdef DIA_DEBUG` |

## Open Design Questions

1. **Ctrl+P while the panel is hidden.** JS key handling only fires when the panel page has input focus. Opening the palette from a hidden panel needs the C++ side (as `VisualDebuggerModule` does for `~`). Proposal: v1 requires the panel to be open; revisit if it is annoying in use.
2. **Ctrl+P collisions.** The modifier path is confirmed in code: `UIModule` passes modifiers to `InjectKeyDown`, which maps `kModControl` to Ultralight's `kMod_CtrlKey`, so the page should see `event.ctrlKey`. What is unverified is whether the host window or Ultralight claims Ctrl+P first. Task 5 checks at runtime; fall back to Ctrl+K if so.
3. **Domain action.** "Reveal the card" is read-only. Is toggling the domain's drawers on from the palette also wanted? Proposal: no; the card is one click away and the toggle semantics differ per domain.
4. **Command metadata quality.** The palette is only as good as `description`/`example`. Some registrants may have empty descriptions. Proposal: show them anyway and list offenders in the plan's final task notes.
