# Feature Spec: Undo/Redo

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Undo/Redo** | (this document) |

## Problem Statement

Editors need undo/redo to let users safely experiment with changes. Without it, every edit is permanent until save — one wrong drag in the FlowView or a bad JSON edit in ModuleInspector can only be fixed by manually reversing the change or reloading from disk. This is a framework-level concern because every plugin (DiaApplicationEditor, future DiaGraphicsEditor, etc.) needs the same undo/redo infrastructure.

## Acceptance Criteria

- [ ] IEditorCommand interface with Execute() and Undo() methods
- [ ] CommandHistory module tracks executed commands in a stack
- [ ] Undo reverses the most recent command, Redo re-applies it
- [ ] New commands after an undo clear the redo stack (standard behavior)
- [ ] Plugins create commands by implementing IEditorCommand
- [ ] CommandHistory is a Module within EditorApplication (participates in lifecycle)
- [ ] EditorModel dirty flag updated on undo/redo (undo past save point = clean)
- [ ] Ctrl+Z / Ctrl+Y keyboard shortcuts wired through EditorViewController
- [ ] Compound commands group multiple operations into a single undo step
- [ ] BeginCompound/EndCompound for interactive multi-step grouping (auto-closes on next frame if unpaired)
- [ ] CommandHistory has configurable max depth (default 100)
- [ ] UI can query undo/redo availability and command descriptions (for menu text)

## Design

### IEditorCommand Interface

```cpp
namespace Dia::Editor {
    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;

        virtual void Execute() = 0;
        virtual void Undo() = 0;

        virtual const char* GetDescription() const = 0;
    };
}
```

Each command captures enough state to reverse itself. Commands own their undo data — the framework doesn't snapshot the model. Commands are also responsible for calling `NotifyObservers()` on the specific data paths they mutate, in both `Execute()` and `Undo()`. This keeps UI updates granular — only affected views re-render.

### CompoundCommand

Groups multiple commands into a single undo step (e.g., "add phase + connect transition" as one operation).

```cpp
namespace Dia::Editor {
    class CompoundCommand : public IEditorCommand {
    public:
        CompoundCommand(const char* description);

        void AddSubCommand(IEditorCommand* command);

        void Execute() override;  // Executes sub-commands in order
        void Undo() override;     // Undoes sub-commands in reverse order

        const char* GetDescription() const override;

    private:
        const char* mDescription;
        DynamicArrayC<IEditorCommand*, 8> mSubCommands;
    };
}
```

### CommandHistory Module

```cpp
namespace Dia::Editor {
    class CommandHistory : public Dia::Application::Module {
    public:
        CommandHistory(int maxDepth = 100);

        // Execute a command and push it onto the undo stack.
        // If inside a BeginCompound/EndCompound block, the command executes
        // immediately but is collected into the active compound group.
        void ExecuteCommand(IEditorCommand* command);

        // Interactive compound grouping. Between Begin/End, individual
        // ExecuteCommand() calls execute immediately (user sees feedback)
        // but are grouped into a single undo step.
        // Auto-closes on next frame if EndCompound() is not called.
        void BeginCompound(const char* description);
        void EndCompound();
        bool IsCompoundActive() const;

        // Undo/redo
        bool CanUndo() const;
        bool CanRedo() const;
        void Undo();
        void Redo();

        // For UI display ("Undo Add Phase", "Redo Delete Module")
        const char* GetUndoDescription() const;
        const char* GetRedoDescription() const;

        // Clear all history (e.g., on project close)
        void Clear();

        // Save point tracking
        void MarkSavePoint();
        bool IsAtSavePoint() const;

    private:
        DynamicArrayC<IEditorCommand*, 128> mUndoStack;
        DynamicArrayC<IEditorCommand*, 128> mRedoStack;
        int mMaxDepth;
        int mSavePointIndex;  // Index in undo stack at last save, -1 if never saved
        CompoundCommand* mActiveCompound;  // Non-null between Begin/End
    };
}
```

### Save Point Integration

CommandHistory tracks which position in the undo stack corresponds to the last save. This drives the dirty flag:

- `ExecuteCommand()` — model becomes dirty (unless at save point)
- `Undo()` — if undoing back to save point, model becomes clean
- `Redo()` — if redoing to save point, model becomes clean
- `MarkSavePoint()` — called by EditorModel::SaveProject(), records current stack position
- New commands after undo clear the redo stack; if save point was in the redo stack, it's lost (model stays dirty until next save)

### Integration with EditorViewController

```cpp
// EditorViewController wires keyboard shortcuts and UI commands
void EditorViewController::OnUIEvent(const StringCRC& eventType, const Json::Value& data) {
    if (eventType == StringCRC("undo")) {
        mCommandHistory->Undo();
        mModel->NotifyObservers(StringCRC("undo_redo_state"));
    }
    else if (eventType == StringCRC("redo")) {
        mCommandHistory->Redo();
        mModel->NotifyObservers(StringCRC("undo_redo_state"));
    }
}

// ExecuteCommand routes through CommandHistory instead of directly mutating model
void EditorViewController::ExecuteEditorCommand(IEditorCommand* command) {
    mCommandHistory->ExecuteCommand(command);
    mModel->NotifyObservers(StringCRC("undo_redo_state"));
}
```

### Plugin Usage Example (DiaApplicationEditor)

```cpp
namespace Dia::Application::Editor {
    class AddPhaseTransitionCommand : public Dia::Editor::IEditorCommand {
    public:
        AddPhaseTransitionCommand(ManifestEditorData* data,
                                  Dia::Editor::EditorModel* model,
                                  const StringCRC& fromPhase,
                                  const StringCRC& toPhase)
            : mData(data), mModel(model), mFromPhase(fromPhase), mToPhase(toPhase) {}

        void Execute() override {
            mData->manifest->AddTransition(mFromPhase, mToPhase);
            mModel->NotifyObservers(StringCRC("manifest_transitions"));
        }

        void Undo() override {
            mData->manifest->RemoveTransition(mFromPhase, mToPhase);
            mModel->NotifyObservers(StringCRC("manifest_transitions"));
        }

        const char* GetDescription() const override {
            return "Add Phase Transition";
        }

    private:
        ManifestEditorData* mData;
        Dia::Editor::EditorModel* mModel;
        StringCRC mFromPhase;
        StringCRC mToPhase;
    };

    // When user drags a connection in FlowView:
    void DiaApplicationEditor::OnAddTransition(const StringCRC& from, const StringCRC& to) {
        auto* cmd = new AddPhaseTransitionCommand(mPluginData, mEditorModel, from, to);
        mEditorModel->GetCommandHistory()->ExecuteCommand(cmd);
    }
}
```

### Interactive Compound Example

For multi-step interactive operations (e.g., delete selected nodes), BeginCompound/EndCompound groups immediate commands into a single undo step:

```cpp
// User selects 3 modules and hits Delete:
void DiaApplicationEditor::OnDeleteSelection(const DynamicArrayC<StringCRC, 16>& selectedIds) {
    auto* history = mEditorModel->GetCommandHistory();
    history->BeginCompound("Delete Selection");

    for (int i = 0; i < selectedIds.Size(); ++i) {
        auto* cmd = new RemoveModuleCommand(mPluginData, selectedIds[i]);
        history->ExecuteCommand(cmd);  // Executes immediately, user sees module disappear
    }

    history->EndCompound();  // All 3 removals undo as one step
}
```

If `EndCompound()` is never called (e.g., exception, plugin bug), CommandHistory auto-closes the compound on the next `DoUpdate()` frame. The commands still undo — just as a group rather than being lost.

### UI Integration (React)

```typescript
// Undo/redo state pushed from C++ via WebUIBridge
interface UndoRedoState {
    canUndo: boolean;
    canRedo: boolean;
    undoDescription: string | null;  // "Add Phase Transition"
    redoDescription: string | null;  // "Delete Module"
}

// Toolbar buttons
<button disabled={!undoRedo.canUndo} onClick={() => dia.sendMessage("undo", {})}>
    Undo {undoRedo.undoDescription}
</button>
<button disabled={!undoRedo.canRedo} onClick={() => dia.sendMessage("redo", {})}>
    Redo {undoRedo.redoDescription}
</button>

// Keyboard shortcuts handled by EditorViewController (not JS)
// Ctrl+Z -> "undo" event, Ctrl+Y -> "redo" event
```

### Memory Management

Commands are heap-allocated and owned by CommandHistory. When commands fall off the bottom of the undo stack (exceeding max depth) or when the redo stack is cleared, CommandHistory deletes them. CompoundCommand deletes its sub-commands.

## Implementation Files

- `Dia/DiaEditor/Command/IEditorCommand.h` - Command interface
- `Dia/DiaEditor/Command/CompoundCommand.h` - Compound command
- `Dia/DiaEditor/Command/CompoundCommand.cpp` - Compound command implementation
- `Dia/DiaEditor/Command/CommandHistory.h` - History module interface
- `Dia/DiaEditor/Command/CommandHistory.cpp` - History module implementation

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | **Compliant** — event types (`"undo"`, `"redo"`, `"undo_redo_state"`) are StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — CommandHistory is a Module |
| Platform | PD-003 | Component-based entities | **N/A** — undo/redo is infrastructure, not an entity system |
| Platform | PD-004 | No STL containers in public APIs | **Compliant** — uses DynamicArrayC, `const char*` |
| Platform | PD-006 | Visual Studio project files are source of truth | **Compliant** — implementation files added to DiaEditor .vcxproj |
| Dia | AD-001 | Module system with YAML frontmatter | **N/A** — CommandHistory is a Module within DiaEditor, not a standalone module needing its own doc |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — all types in `Dia::Editor::` |
| Dia | AD-004 | PU/Phase/Module for app structure | **Compliant** — CommandHistory extends Module |
| Dia | AD-005 | Component-based entities | **N/A** — same as PD-003 |
| DiaEditor | SED-001 | Plugin interface minimal and stable | **Compliant** — IEditorCommand is separate from IEditorPlugin; plugins opt-in to command pattern |
| DiaEditor | SED-002 | Plugins register via macro | **N/A** — undo/redo is framework, not a plugin |
| DiaEditor | SED-003 | Systems own editors as `<System>/Editor/` | **N/A** — undo/redo is framework, not a system editor |
| DiaEditor | SED-004 | WebSocket protocol uses JSON | **N/A** — undo/redo is local, not networked |
| DiaEditor | SED-005 | CEF replaces Awesomium | **N/A** — no direct UI rendering dependency |
| DiaEditor | SED-006 | Docking managed by react-mosaic | **N/A** — no layout concern |
| DiaEditor | SED-007 | CommandDispatcher embeds Python | **N/A** — IEditorCommand is a different "command" concept than DiaCLI commands |
| DiaEditor | SED-008 | EditorModel uses Observer pattern | **Compliant** — undo/redo notifies via `"undo_redo_state"` data path |
| DiaEditor | SED-009 | Framework provides undo/redo | **Compliant** — this IS the implementation |
| DiaEditor | SED-010 | Use DiaDebugProtocol for wire types | **N/A** — undo/redo is local, not networked |

**All binding decisions: COMPLIANT (no conflicts)**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Memory | Who owns command objects? | CommandHistory owns and deletes them | CommandHistory takes ownership on ExecuteCommand(). Deletes when stack overflows or clears. |
| 2 | Threading | Is CommandHistory thread-safe? | No — all editor operations happen on main thread | Single-threaded. EditorViewController serializes all command execution. |
| 3 | Serialization | Should command history persist across sessions? | No — clear on project close | History is transient. Reloading a project starts with empty history. |
| 4 | Live debugging | Undo during live connection? | Undo only affects local model, not game | Undo reverses local edits. Runtime config pushes already sent to game are NOT undone — that would require sending a reverse command to the game, which is a separate concern. |
| 5 | Limits | Max undo depth? | 100 commands (configurable) | 100 default. Large commands (e.g., full manifest replacement) count as 1. CompoundCommand counts as 1. |
| 6 | Dirty flag | Does undo to save point clear dirty? | Yes | IsAtSavePoint() tracks this. Classic editor behavior — undo past save = clean, edit after = dirty. |
| 7 | Compound | How to group interactive multi-step operations for undo? | BeginCompound/EndCompound on CommandHistory | Commands between Begin/End execute immediately but undo as one step. Auto-closes on next frame if EndCompound not called. Explicit CompoundCommand still available for programmatic batches. |
| 8 | Notifications | Who fires model data notifications on undo/redo? | The command itself | Each command calls `NotifyObservers()` on the specific data paths it mutates, in both Execute() and Undo(). Keeps updates granular — only affected views re-render. CommandHistory additionally notifies `"undo_redo_state"` for toolbar state. |

## Status

`Approved` - Ready for implementation
