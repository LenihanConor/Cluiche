# Feature Spec: Undo/Redo

## Parent System
@docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-004, PD-007 |
| Application | @docs/specs/applications/dia/dia.md | AD-003 |
| System | @docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md | ED-007 |

## Purpose

Provide a command-pattern undo/redo system that records every edit to the ManifestDocument as a reversible command. This enables safe editing — users can explore changes freely knowing they can revert. All other editing features (PU Inspector, Module Inspector, Stage Configuration, etc.) issue commands through this system rather than mutating the model directly.

## Acceptance Criteria

1. **Command pattern** — Every mutation to ManifestDocument is represented as a `Command` object with `Execute()` and `Undo()` methods.
2. **Undo (Ctrl+Z)** — Reverts the most recent command. Model returns to prior state. UI updates immediately.
3. **Redo (Ctrl+Y)** — Re-applies the most recently undone command. Redo stack cleared when a new command is executed after undo.
4. **Stack depth: 100** — Oldest commands are dropped when stack exceeds 100 entries.
5. **Stack cleared on reload** — Loading a new file or reloading clears the entire undo/redo history.
6. **Dirty integration** — Undo back to the save-point clears `isDirty`. Any deviation from save-point sets `isDirty`.
7. **History display** — React UI shows command history list with the current position highlighted. Clicking an entry jumps to that state (multi-undo/redo).

## Design

### Command Interface

```cpp
namespace Dia::ApplicationFlow::Editor {
    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute(ManifestDocument& doc) = 0;
        virtual void Undo(ManifestDocument& doc) = 0;
        virtual const char* GetDescription() const = 0;
    };
}
```

### CommandHistory

```cpp
namespace Dia::ApplicationFlow::Editor {
    class CommandHistory {
    public:
        static constexpr unsigned int kMaxCommands = 100;

        void Execute(ICommand* command, ManifestDocument& doc);
        bool CanUndo() const;
        bool CanRedo() const;
        void Undo(ManifestDocument& doc);
        void Redo(ManifestDocument& doc);
        void Clear();

        unsigned int GetCurrentIndex() const;
        unsigned int GetCount() const;
        const ICommand* GetCommand(unsigned int index) const;

        void SetSavePoint();
        bool IsAtSavePoint() const;

    private:
        // Ring buffer or shifting array of command pointers
        unsigned int mCurrentIndex;
        unsigned int mSavePointIndex;
    };
}
```

### Save-Point Tracking

`SetSavePoint()` is called after a successful save. `IsAtSavePoint()` drives `isDirty` — when the current index equals the save-point index, the document is clean. This correctly handles the case where the user undoes back to the saved state.

### Compound Commands

Some edits (e.g., "remove module" which also removes its stream references) require multiple atomic changes. A `CompoundCommand` groups child commands and executes/undoes them as one unit — one entry in the history.

```cpp
class CompoundCommand : public ICommand {
public:
    void Add(ICommand* child);
    void Execute(ManifestDocument& doc) override;  // executes all children in order
    void Undo(ManifestDocument& doc) override;     // undoes all children in reverse order
    const char* GetDescription() const override;   // description of the compound action
};
```

### Frontend Communication

- `editor.history.undo()` / `editor.history.redo()` — trigger from keyboard shortcuts or UI
- `editor.history.getState()` — returns `{ commands: [{description, index}...], currentIndex, canUndo, canRedo }`
- `editor.history.jumpTo(index)` — multi-step undo/redo to reach target
- Events: `onHistoryChanged` — UI re-renders history list and undo/redo button states

### Keyboard Shortcuts

Ctrl+Z and Ctrl+Y handled at the editor panel level (React `onKeyDown`). Shortcuts are active when the ApplicationFlowEditor panel is focused.

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | Define `ICommand` interface and `CommandHistory` class | Unit test: execute/undo/redo, stack overflow at 100, clear | Todo | Core infrastructure |
| 2 | Implement `CompoundCommand` | Unit test: compound executes all, undo reverses all | Todo | |
| 3 | Implement save-point tracking and isDirty integration | Unit test: save-point set, undo to save-point clears dirty | Todo | Integrates with Manifest Load/Save |
| 4 | CEF message handlers for history operations | Integration test: message round-trip | Todo | |
| 5 | React history panel + Ctrl+Z/Y shortcuts | Manual test via editor | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | CommandHistory uses DiaCore containers internally. ICommand interface is engine-native. |
| PD-007 | C++20 required | Implementation uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | All code in `Dia::ApplicationFlow::Editor::` namespace. |
| ED-007 | React + CEF frontend | History UI is React; backend communication via CEF message passing. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Memory | How are Command objects owned/allocated? | CommandHistory owns them. Allocated via `new` (editor is not perf-critical path). Freed when dropped from stack or on Clear(). |
| 2 | History | Should the history panel show a flat list or group compound commands? | Flat list — each entry is one user action. CompoundCommand shows its top-level description, not child details. |
| 3 | Shortcuts | Should Ctrl+Shift+Z be an alternative redo shortcut? | Yes — common convention. Both Ctrl+Y and Ctrl+Shift+Z trigger redo. |
| 4 | JumpTo | What happens to redo stack when jumping backward via history click? | Same as sequential undo — redo stack preserved. A new command after jump clears the redo portion (same as normal undo→new-edit). |

## Status

`Approved` — 2026-05-19
