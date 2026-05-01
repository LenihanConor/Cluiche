# Feature Spec: event-system

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **event-system** |

**Status:** `Done`

---

## Problem Statement

Provide observable lifecycle events for command registration and execution so external systems can log, monitor, debug, and react to CLI operations without coupling to DiaAPI internals.

---

## Solution Overview

The **event-system** feature implements the Observer pattern using DiaCore's Observer/ObserverSubject infrastructure. It defines 5 event types (OnCommandRegistered, OnCommandExecuting, OnCommandExecuted, OnCommandError, OnHelpRequested) that are fired at key points in the CLI lifecycle. Systems can subscribe to events to implement logging, telemetry, debugging, or custom behavior without modifying DiaAPI code.

### Key Design Points

1. **DiaCore Observer pattern** - Use existing DiaCore/Architecture/Observer infrastructure
2. **5 lifecycle events** - Registration, before execution, after execution, errors, help
3. **Event data structs** - Strongly-typed event payloads (not void pointers)
4. **Fire-and-forget** - Events are informational, observers can't block execution
5. **Thread-safe** - Observer pattern handles thread safety via DiaCore implementation

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Fire OnCommandRegistered event when RegisterCommand() succeeds | Unit test: Subscribe to event, register command, verify observer called |
| AC2 | Fire OnCommandExecuting event before command callback is invoked | Unit test: Subscribe, execute command, verify event fired before callback |
| AC3 | Fire OnCommandExecuted event after command callback completes | Unit test: Subscribe, execute command, verify event fired after callback with exit code and duration |
| AC4 | Fire OnCommandError event when command throws or returns error | Unit test: Command returns non-zero, verify OnCommandError fired |
| AC5 | Fire OnHelpRequested event when help is displayed | Unit test: Subscribe, show help, verify event fired |
| AC6 | Multiple observers can subscribe to same event | Unit test: 2 observers subscribe to OnCommandExecuted, both called |
| AC7 | Event payloads include relevant data (command name, exit code, duration, etc.) | Unit test: Verify event data fields populated correctly |
| AC8 | Events use DiaCore Observer pattern (not custom implementation) | Code review: Verify uses Observer/ObserverSubject |

---

## Public API

### Event Data Structures

```cpp
namespace Dia::API {

// Event: Command registered
struct CommandRegisteredEvent {
    Dia::Core::StringCRC commandName;
    const char* description;
};

// Event: Command about to execute
struct CommandExecutingEvent {
    Dia::Core::StringCRC commandName;
    const CommandArgs* args;  // Pointer to arguments (not owned)
};

// Event: Command execution completed
struct CommandExecutedEvent {
    Dia::Core::StringCRC commandName;
    int exitCode;
    float durationSeconds;
};

// Event: Command execution error
struct CommandErrorEvent {
    Dia::Core::StringCRC commandName;
    const char* errorMessage;
    int exitCode;
};

// Event: Help requested
struct HelpRequestedEvent {
    Dia::Core::StringCRC commandName;  // Empty if global help
    bool isGlobalHelp;
};

}
```

### Event Subjects (Observable)

```cpp
namespace Dia::API {

// Get event subjects for subscribing
Dia::Core::ObserverSubject<CommandRegisteredEvent>& GetCommandRegisteredSubject();
Dia::Core::ObserverSubject<CommandExecutingEvent>& GetCommandExecutingSubject();
Dia::Core::ObserverSubject<CommandExecutedEvent>& GetCommandExecutedSubject();
Dia::Core::ObserverSubject<CommandErrorEvent>& GetCommandErrorSubject();
Dia::Core::ObserverSubject<HelpRequestedEvent>& GetHelpRequestedSubject();

}
```

### Internal: Fire Events

```cpp
namespace Dia::API::Internal {

// Fire events (called by other DiaAPI features)
void FireCommandRegistered(const Dia::Core::StringCRC& name, const char* description);
void FireCommandExecuting(const Dia::Core::StringCRC& name, const CommandArgs* args);
void FireCommandExecuted(const Dia::Core::StringCRC& name, int exitCode, float duration);
void FireCommandError(const Dia::Core::StringCRC& name, const char* errorMessage, int exitCode);
void FireHelpRequested(const Dia::Core::StringCRC& commandName, bool isGlobalHelp);

}
```

---

## Implementation Notes

### Observer Pattern Usage

```cpp
// Subscribe to event
class MyLogger : public Dia::Core::Observer<CommandExecutedEvent> {
    void Notify(const CommandExecutedEvent& event) override {
        printf("Command %s completed with code %d in %.3fs\n", 
            event.commandName.GetDebugName(), 
            event.exitCode, 
            event.durationSeconds);
    }
};

MyLogger logger;
GetCommandExecutedSubject().Attach(&logger);
```

### Internal State

```cpp
namespace Dia::API::Internal {

struct EventSystemState {
    Dia::Core::ObserverSubject<CommandRegisteredEvent> commandRegisteredSubject;
    Dia::Core::ObserverSubject<CommandExecutingEvent> commandExecutingSubject;
    Dia::Core::ObserverSubject<CommandExecutedEvent> commandExecutedSubject;
    Dia::Core::ObserverSubject<CommandErrorEvent> commandErrorSubject;
    Dia::Core::ObserverSubject<HelpRequestedEvent> helpRequestedSubject;
};

extern EventSystemState gEventSystemState;

}
```

### Integration Points

**command-registry feature:**
- `RegisterCommand()` calls `FireCommandRegistered()` on success

**help-system feature:**
- `ShowGlobalHelp()` and `ShowCommandHelp()` call `FireHelpRequested()`

**DiaAPI main (future ExecuteCommand):**
- Before command callback: `FireCommandExecuting()`
- After command callback: `FireCommandExecuted()` or `FireCommandError()`
- Measure duration with `std::chrono::high_resolution_clock`

---

## Dependencies

### Required Modules
- **DiaCore/Architecture/Observer** - Observer, ObserverSubject
- **DiaCore/CRC** - StringCRC
- **command-registry** - CommandArgs definition

### Dependent Features
- **command-registry** - Fires CommandRegistered
- **help-system** - Fires HelpRequested
- **DiaAPI main** - Fires CommandExecuting, CommandExecuted, CommandError

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/CLI/TestEventSystem.cpp)

1. **Event subscription**
   - Create Observer, attach to subject
   - Fire event, verify observer called

2. **CommandRegistered event**
   - Subscribe to GetCommandRegisteredSubject()
   - Call FireCommandRegistered()
   - Verify event data (name, description)

3. **CommandExecuting event**
   - Subscribe, fire event with CommandArgs
   - Verify event data (name, args pointer)

4. **CommandExecuted event**
   - Subscribe, fire event with exit code and duration
   - Verify event data (name, exitCode, durationSeconds)

5. **CommandError event**
   - Subscribe, fire event with error message
   - Verify event data (name, errorMessage, exitCode)

6. **HelpRequested event**
   - Subscribe, fire event with command name
   - Verify event data (commandName, isGlobalHelp)

7. **Multiple observers**
   - Attach 3 observers to same subject
   - Fire event, verify all 3 called

8. **Observer detach**
   - Attach observer, fire event (verify called)
   - Detach observer, fire event (verify not called)

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Event structs use StringCRC for command names. |
| PD-004 | Platform | No STL containers in public APIs | ✅ **Compliant** - Event structs use StringCRC and pointers, no STL. |
| PD-006 | Platform | Visual Studio project files are source of truth | ✅ **Compliant** - Part of DiaAPI.vcxproj. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | ✅ **Compliant** - Part of DiaAPI module documentation. |
| AD-002 | Dia App | No STL containers in public APIs | ✅ **Compliant** - Reinforces PD-004. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::API::` namespace. |
| SD-004 | DiaAPI System | No interactive prompts (headless by default) | ✅ **Compliant** - Events are fire-and-forget, no blocking. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Should we use DiaCore Observer or custom implementation? | ✅ DiaCore Observer - proven, thread-safe, consistent with platform |
| 2 | Event Timing | Should CommandExecuted fire even if command throws? | ✅ No - CommandError fires instead. Executed = success or non-zero return, Error = exception/crash |
| 3 | Event Data | Should we pass CommandInfo* or just name/description? | ✅ Just name/description - minimal payload, observers can query registry if needed |
| 4 | Thread Safety | Are events thread-safe? | ✅ Yes - DiaCore ObserverSubject handles thread safety |
| 5 | Performance | Should events be optional (compile-time disable)? | ✅ No - events are lightweight, always enabled for observability |

---

## Open Questions

All resolved:

1. ✅ **Event order guarantees:** Events fire in order (CommandExecuting before CommandExecuted), but observer notification order is undefined (DiaCore behavior)
2. ✅ **Error vs Executed:** If command returns non-zero, fire CommandExecuted (not CommandError). CommandError only for exceptions.
3. ✅ **Help events:** Fire HelpRequested before showing help text (allows observers to log/track help usage)

---

## Implementation Plan

### Phase 1: Event Infrastructure (1 day)
- Define event structs
- Create GetXSubject() functions
- Create FireX() internal functions
- Unit tests for event firing

### Phase 2: Integration with command-registry (0.5 days)
- Add FireCommandRegistered() call to RegisterCommand()
- Unit tests for registration events

### Phase 3: Integration with help-system (0.5 days)
- Add FireHelpRequested() calls to ShowHelp()
- Unit tests for help events

### Phase 4: Execution Events (1 day)
- Add FireCommandExecuting/Executed/Error to ExecuteCommand() (future work, stub for now)
- Unit tests for execution events
- Duration measurement with chrono

**Total Estimate:** 3 days

---

## Examples

### Example 1: Simple Event Logger

```cpp
#include <DiaAPI/Events/EventSystem.h>

using namespace Dia::API;
using namespace Dia::Core;

class CommandLogger : public Observer<CommandExecutedEvent> {
public:
    void Notify(const CommandExecutedEvent& event) override {
        printf("[LOG] Command '%s' completed: exit=%d, duration=%.3fs\n",
            event.commandName.GetDebugName(),
            event.exitCode,
            event.durationSeconds);
    }
};

int main() {
    Initialize();
    
    CommandLogger logger;
    GetCommandExecutedSubject().Attach(&logger);
    
    // Now all command executions will be logged
    RunCLI(argc, argv);
    
    Shutdown();
    return 0;
}
```

### Example 2: Multiple Event Observers

```cpp
#include <DiaAPI/Events/EventSystem.h>

using namespace Dia::API;
using namespace Dia::Core;

class TelemetryTracker : public Observer<CommandExecutingEvent> {
public:
    void Notify(const CommandExecutingEvent& event) override {
        // Send to telemetry service
    }
};

class DebugTracer : public Observer<CommandExecutingEvent> {
public:
    void Notify(const CommandExecutingEvent& event) override {
        printf("[DEBUG] Executing: %s\n", event.commandName.GetDebugName());
    }
};

int main() {
    Initialize();
    
    TelemetryTracker telemetry;
    DebugTracer tracer;
    
    GetCommandExecutingSubject().Attach(&telemetry);
    GetCommandExecutingSubject().Attach(&tracer);
    
    // Both observers will be notified for every command
    RunCLI(argc, argv);
    
    Shutdown();
    return 0;
}
```

### Example 3: Error Monitoring

```cpp
#include <DiaAPI/Events/EventSystem.h>

using namespace Dia::API;
using namespace Dia::Core;

class ErrorMonitor : public Observer<CommandErrorEvent> {
public:
    void Notify(const CommandErrorEvent& event) override {
        DIA_LOG_ERROR("DiaAPI", "Command '%s' failed: %s (code %d)",
            event.commandName.GetDebugName(),
            event.errorMessage,
            event.exitCode);
        
        // Could also: send alert, write crash dump, etc.
    }
};

int main() {
    Initialize();
    
    ErrorMonitor monitor;
    GetCommandErrorSubject().Attach(&monitor);
    
    RunCLI(argc, argv);
    
    Shutdown();
    return 0;
}
```

---

## Status

`Done` - Implemented
