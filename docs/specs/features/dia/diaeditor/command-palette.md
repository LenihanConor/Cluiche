# Feature Spec: Command Palette

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Command Palette** | (this document) |

## Problem Statement

Provides fuzzy-search command execution interface (like VS Code Command Palette) allowing users to discover and execute DiaCLI commands via keyboard without navigating menus. Supports fuzzy matching with ranking, inline argument prompts, and persistent command history.

## Acceptance Criteria

- [ ] Ctrl+Shift+P keyboard shortcut opens palette
- [ ] Fuzzy search with ranking (subsequence match + quality scoring)
- [ ] Auto-discover all commands from DiaAPI CommandRegistry
- [ ] Optional metadata override for better UI (display names, categories, icons)
- [ ] Inline argument prompts (VS Code style - sequential input)
- [ ] Persist command history to `~/.cluiche/command-history.json`
- [ ] Show recent commands at top of list
- [ ] Keyboard navigation (arrow keys, Enter to execute, Esc to cancel)
- [ ] Execute command and show result in Output Console
- [ ] Loading indicator for long-running commands

## Design

### CommandPalette React Component

**UI Component:**
```typescript
// Cluiche/CluicheEditor/UI/src/components/CommandPalette.tsx
import React, { useState, useEffect, useRef } from 'react';
import Fuse from 'fuse.js';  // Fuzzy search library with ranking

interface Command {
    id: string;
    displayName: string;
    description?: string;
    category?: string;
    icon?: string;
    args?: CommandArg[];
}

interface CommandArg {
    name: string;
    type: 'string' | 'path' | 'boolean';
    required: boolean;
    default?: any;
}

export const CommandPalette: React.FC = () => {
    const [isOpen, setIsOpen] = useState(false);
    const [searchQuery, setSearchQuery] = useState('');
    const [commands, setCommands] = useState<Command[]>([]);
    const [filteredCommands, setFilteredCommands] = useState<Command[]>([]);
    const [recentCommands, setRecentCommands] = useState<string[]>([]);
    const [selectedIndex, setSelectedIndex] = useState(0);
    const [argPromptMode, setArgPromptMode] = useState(false);
    const [currentCommand, setCurrentCommand] = useState<Command | null>(null);
    const [argIndex, setArgIndex] = useState(0);
    const [argValues, setArgValues] = useState<any[]>([]);
    
    const fuseRef = useRef<Fuse<Command> | null>(null);
    
    useEffect(() => {
        // Load commands from C++ (Decision 30: Auto-discover from CommandRegistry)
        window.CluicheEditor.getCommands().then((cmdList: Command[]) => {
            setCommands(cmdList);
            
            // Initialize fuzzy search (Decision 28: Fuzzy + ranking)
            fuseRef.current = new Fuse(cmdList, {
                keys: ['displayName', 'description', 'category'],
                includeScore: true,
                threshold: 0.4,  // Fuzzy tolerance
                ignoreLocation: true
            });
        });
        
        // Load command history (Decision 32: Persist to disk)
        window.CluicheEditor.getCommandHistory().then((history: string[]) => {
            setRecentCommands(history);
        });
        
        // Keyboard shortcut (Decision 29: Ctrl+Shift+P)
        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.ctrlKey && e.shiftKey && e.key === 'P') {
                e.preventDefault();
                setIsOpen(true);
            }
            if (e.key === 'Escape' && isOpen) {
                setIsOpen(false);
                resetState();
            }
        };
        
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, [isOpen]);
    
    useEffect(() => {
        if (!searchQuery) {
            // Show recent commands at top when no search
            const recentCmds = commands.filter(cmd => recentCommands.includes(cmd.id));
            const otherCmds = commands.filter(cmd => !recentCommands.includes(cmd.id));
            setFilteredCommands([...recentCmds, ...otherCmds]);
        } else {
            // Fuzzy search with ranking
            const results = fuseRef.current?.search(searchQuery) || [];
            setFilteredCommands(results.map(r => r.item));
        }
        setSelectedIndex(0);
    }, [searchQuery, commands, recentCommands]);
    
    const handleExecute = () => {
        const command = filteredCommands[selectedIndex];
        if (!command) return;
        
        // Check if command needs arguments (Decision 31: Inline argument prompts)
        if (command.args && command.args.length > 0) {
            setCurrentCommand(command);
            setArgPromptMode(true);
            setArgIndex(0);
            setArgValues([]);
            setSearchQuery('');  // Clear search, show first arg prompt
        } else {
            // Execute immediately (no args)
            executeCommand(command.id, {});
            setIsOpen(false);
            resetState();
        }
    };
    
    const handleArgSubmit = (value: any) => {
        const newArgValues = [...argValues, value];
        setArgValues(newArgValues);
        
        if (argIndex + 1 < currentCommand!.args!.length) {
            // More args needed, prompt for next
            setArgIndex(argIndex + 1);
            setSearchQuery('');
        } else {
            // All args collected, execute command
            const argsObj = {};
            currentCommand!.args!.forEach((arg, i) => {
                argsObj[arg.name] = newArgValues[i];
            });
            
            executeCommand(currentCommand!.id, argsObj);
            setIsOpen(false);
            resetState();
        }
    };
    
    const executeCommand = (commandId: string, args: any) => {
        // Add to history
        window.CluicheEditor.addToHistory(commandId);
        
        // Execute via C++
        window.CluicheEditor.executeCommand(commandId, args).then((result) => {
            // Result displayed in Output Console (separate feature)
            console.log('Command executed:', commandId, result);
        });
    };
    
    const resetState = () => {
        setSearchQuery('');
        setArgPromptMode(false);
        setCurrentCommand(null);
        setArgIndex(0);
        setArgValues([]);
    };
    
    if (!isOpen) return null;
    
    return (
        <div className="command-palette-overlay">
            <div className="command-palette">
                {!argPromptMode ? (
                    <>
                        <input
                            type="text"
                            placeholder="Search commands..."
                            value={searchQuery}
                            onChange={(e) => setSearchQuery(e.target.value)}
                            onKeyDown={(e) => {
                                if (e.key === 'ArrowDown') {
                                    setSelectedIndex((selectedIndex + 1) % filteredCommands.length);
                                } else if (e.key === 'ArrowUp') {
                                    setSelectedIndex((selectedIndex - 1 + filteredCommands.length) % filteredCommands.length);
                                } else if (e.key === 'Enter') {
                                    handleExecute();
                                }
                            }}
                            autoFocus
                        />
                        <div className="command-list">
                            {filteredCommands.map((cmd, index) => (
                                <div
                                    key={cmd.id}
                                    className={`command-item ${index === selectedIndex ? 'selected' : ''}`}
                                    onClick={() => { setSelectedIndex(index); handleExecute(); }}
                                >
                                    {cmd.icon && <span className="icon">{cmd.icon}</span>}
                                    <span className="name">{cmd.displayName}</span>
                                    {cmd.category && <span className="category">{cmd.category}</span>}
                                </div>
                            ))}
                        </div>
                    </>
                ) : (
                    <>
                        <div className="arg-prompt">
                            <span className="command-name">{currentCommand!.displayName}</span>
                            <span className="arg-name">{currentCommand!.args![argIndex].name}:</span>
                        </div>
                        <input
                            type="text"
                            placeholder={`Enter ${currentCommand!.args![argIndex].name}...`}
                            value={searchQuery}
                            onChange={(e) => setSearchQuery(e.target.value)}
                            onKeyDown={(e) => {
                                if (e.key === 'Enter') {
                                    handleArgSubmit(searchQuery);
                                }
                            }}
                            autoFocus
                        />
                    </>
                )}
            </div>
        </div>
    );
};
```

### C++ CommandPalette Support

**CommandDispatcher (C++ Side):**
```cpp
namespace Dia::Editor {
    class CommandDispatcher : public Dia::Application::Module {
    public:
        // Get all available commands (Decision 30: Auto-discover from CommandRegistry)
        Json::Value GetAllCommands() const;
        
        // Execute command
        int ExecuteCommand(const char* commandId, const Json::Value& args);
        
        // Command history (Decision 32: Persist to ~/.cluiche/command-history.json)
        void AddToHistory(const char* commandId);
        Json::Value GetHistory() const;
        void SaveHistory();
        void LoadHistory();
        
        // Metadata override (Decision 30: Optional metadata)
        void SetCommandMetadata(const char* commandId, const Json::Value& metadata);
        
    private:
        DynamicArrayC<StringCRC, 50> mCommandHistory;
        HashTable<StringCRC, Json::Value> mCommandMetadata;
    };
}
```

**GetAllCommands:**
```cpp
Json::Value CommandDispatcher::GetAllCommands() const {
    Json::Value commands(Json::arrayValue);
    
    // Auto-discover from DiaAPI CommandRegistry
    const auto& registeredCommands = DiaAPI::CommandRegistry::Instance().GetAllCommands();
    
    for (const auto& cmd : registeredCommands) {
        Json::Value cmdJson;
        cmdJson["id"] = cmd.name.GetString();
        
        // Check for metadata override
        const Json::Value* meta = mCommandMetadata.Find(cmd.name);
        if (meta) {
            // Use custom metadata
            cmdJson["displayName"] = (*meta)["displayName"].asString();
            cmdJson["category"] = meta->get("category", "General").asString();
            cmdJson["icon"] = meta->get("icon", "").asString();
        } else {
            // Use default (command name as display name)
            cmdJson["displayName"] = cmd.name.GetString();
            cmdJson["category"] = "General";
        }
        
        cmdJson["description"] = cmd.description;
        
        // Parse args from command definition
        Json::Value argsJson(Json::arrayValue);
        for (const auto& arg : cmd.args) {
            Json::Value argJson;
            argJson["name"] = arg.name;
            argJson["type"] = arg.type;  // "string", "path", "boolean"
            argJson["required"] = arg.required;
            if (arg.hasDefault) {
                argJson["default"] = arg.defaultValue;
            }
            argsJson.append(argJson);
        }
        cmdJson["args"] = argsJson;
        
        commands.append(cmdJson);
    }
    
    return commands;
}
```

**Command History Persistence:**
```cpp
void CommandDispatcher::AddToHistory(const char* commandId) {
    StringCRC id(commandId);
    
    // Remove if already in history (move to front)
    for (int i = 0; i < mCommandHistory.Size(); ++i) {
        if (mCommandHistory[i] == id) {
            mCommandHistory.RemoveAt(i);
            break;
        }
    }
    
    // Add to front (shift existing entries)
    if (mCommandHistory.Size() >= 50) {
        mCommandHistory.RemoveAt(mCommandHistory.Size() - 1);
    }
    mCommandHistory.InsertAt(0, id);
    
    SaveHistory();
}

void CommandDispatcher::SaveHistory() {
    Json::Value historyJson(Json::arrayValue);
    for (int i = 0; i < mCommandHistory.Size(); ++i) {
        historyJson.append(mCommandHistory[i].GetString());
    }
    
    Dia::Core::FilePath historyPath(GetUserDataPath(), "command-history.json");
    Json::StyledWriter writer;
    WriteFileContents(historyPath.AsCString(), writer.write(historyJson).c_str());
}

void CommandDispatcher::LoadHistory() {
    Dia::Core::FilePath historyPath(GetUserDataPath(), "command-history.json");
    if (!FileExists(historyPath.AsCString())) return;
    
    const char* historyStr = ReadFileContents(historyPath.AsCString());
    Json::Value historyJson;
    Json::Reader reader;
    if (reader.parse(historyStr, historyJson)) {
        mCommandHistory.Clear();
        for (unsigned int i = 0; i < historyJson.size(); ++i) {
            mCommandHistory.Add(StringCRC(historyJson[i].asCString()));
        }
    }
}
```

## Implementation Files

- `Dia/DiaEditor/Commands/CommandDispatcher.h/cpp` - C++ command execution and history
- `Cluiche/CluicheEditor/UI/src/components/CommandPalette.tsx` - React UI component
- `Cluiche/CluicheEditor/UI/package.json` - Add `fuse.js` for fuzzy search

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — command IDs are StringCRC |
| Platform | PD-004 | No STL in public APIs | **Compliant** — uses DynamicArrayC, HashTable, StringCRC |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention | **Compliant** — `Dia::Editor::` |
| DiaEditor | SED-005 | CEF replaces Awesomium | **Compliant** — React UI renders in CEF |
| DiaEditor | SED-007 | CommandDispatcher embeds Python | **Compliant** — forwards to DiaAPI CommandRegistry |
| DiaEditor | SED-008 | Observer pattern | **N/A** — palette is UI-driven, not model-observed |

**All binding decisions: COMPLIANT**

## Open Questions

**Resolved:**
- **Decision 28:** Fuzzy + ranking search (subsequence match with quality scoring via fuse.js)
- **Decision 29:** Ctrl+Shift+P keyboard shortcut (VS Code standard)
- **Decision 30:** Auto-discover commands from CommandRegistry + optional metadata override
- **Decision 31:** Inline argument prompts (VS Code style - sequential input)
- **Decision 32:** Persist history to `~/.cluiche/command-history.json` (last 50 commands)

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Search | Fuzzy or exact match? | Fuzzy with ranking (fuse.js) | ✅ Fuzzy + ranking (Decision 28) |
| 2 | Shortcut | Which keyboard shortcut? | Ctrl+Shift+P (VS Code standard) | ✅ Ctrl+Shift+P (Decision 29) |
| 3 | Discovery | Auto-discover or manual? | Auto-discover + optional metadata | ✅ Auto + metadata (Decision 30) |
| 4 | Arguments | How to prompt for args? | Inline sequential input | ✅ Inline (Decision 31) |
| 5 | History | Persist across sessions? | Yes, to ~/.cluiche/command-history.json | ✅ Persist (Decision 32) |

## Implementation Status

**v0 Shipped** (as of 2026-04-20):

| Area | What's implemented | Notes |
|------|--------------------|-------|
| Keyboard shortcut | Ctrl+Shift+P opens palette | Handled in `main.tsx`; also suppressed in CEF via `CEFClientHandler::OnPreKeyEvent` so Chromium's browser "print" accelerator doesn't fire |
| Command registration | `EditorView::RegisterCommand(const char* id, const char* label)` — manual registration | No DiaAPI CommandRegistry auto-discovery yet |
| Built-in commands | `undo`, `redo`, `log.test` | Registered in `EditorView::Initialize`; more commands added as plugins register them |
| Command fetch | `get_commands` request handler in `WebUIBridge` returns `[{id, label}, ...]` | Via `EditorBridge.getCommands()` |
| Fuzzy search | fuse.js over `label` + `id` | Matches VS Code-style ranking |
| Execute | `execute_command` event handler on C++ side | Routes by `id` StringCRC |
| UI | `CommandPalette.tsx` — input + filtered list, arrow keys, Enter, Esc | Renders id next to label |

**Deferred to future iterations:**

- **DiaAPI CommandRegistry auto-discovery** — currently commands are registered manually via `RegisterCommand`; the spec's `CommandDispatcher` module that wraps `DiaAPI::CommandRegistry` has not been built
- **Inline argument prompts** (Decision 31) — current commands are all zero-arg; no sequential arg-prompt UI
- **Persistent history** (Decision 32) — no `~/.cluiche/command-history.json`; palette always starts fresh
- **Recent-commands-at-top ordering** — no history means no "recent" bucket
- **Metadata override** (category, icon, description) — only `id` and `label` for now
- **`CommandDispatcher` C++ module** — simpler design replaces it: commands live as `EditorView::CommandInfo` entries plus a WebUIBridge event handler that dispatches on id

**Revised C++ surface (what's actually in the tree):**

```cpp
// Dia/DiaEditor/MVC/EditorView.h
struct CommandInfo { char id[64]; char label[96]; };
void EditorView::RegisterCommand(const char* id, const char* label);
// Plus WebUIBridge request handler: "get_commands" → JSON array
// Plus WebUIBridge event handler:   "execute_command" → dispatches by id
```

The richer `CommandDispatcher` module (history persistence, metadata, CommandRegistry bridge) remains the aspirational target when a real command surface beyond undo/redo/log is needed.

## Status

`Approved` - v0 implemented; deferred features tracked above
