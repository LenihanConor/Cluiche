# System Spec: DiaDebugServer

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaDebugServer is a generic WebSocket server system that runs inside games to enable remote debugging by editors. It provides a network gateway to the running game, broadcasting performance metrics, streaming subscribed game state, and forwarding DiaAPI commands from connected editors. The server runs on a separate thread (Debug builds only), handles multiple simultaneous editor connections, and uses JSON over WebSocket for all communication.

## Responsibilities

- **WebSocket Server** - Run server on configurable port (default 8080, Debug builds only)
- **Core Metrics Broadcasting** - Broadcast FPS, memory, frame time every 500ms to all connected editors
- **Subscription Management** - Handle editor subscriptions to specific data types (e.g., "processing_unit_state")
- **Event Broadcasting** - Forward MessageBus events to subscribed editors (e.g., phase transitions)
- **DiaAPI Command Gateway** - Receive commands from editors, forward to DiaAPI CommandRegistry, return results
- **Protocol Commands** - Handle debug protocol commands (subscribe, unsubscribe, get_state)
- **Multi-Connection Support** - Broadcast data to all connected editors simultaneously
- **State Serialization** - Serialize game state (ProcessingUnit, Phase, Module) to JSON
- **Thread Safety** - Protect shared data with DiaCore threading primitives

## Public Interfaces

### Core Classes

**DebugServerModule (Module):**
```cpp
namespace Dia::DebugServer {
    class DebugServerModule : public Dia::Application::Module {
    public:
        DebugServerModule(Dia::Application::ProcessingUnit* pu, const StringCRC& id);
        virtual ~DebugServerModule();
        
        // Module lifecycle
        virtual void DoStart(const IStartData* startData) override;
        virtual void DoUpdate() override;  // Process incoming messages, send queued data
        virtual void DoStop() override;
        
        // Configuration
        void SetPort(uint16_t port);  // Default 8080
        void SetCoreMetricsBroadcastInterval(float seconds);  // Default 0.5s (500ms)
        void EnableAutoStart(bool enable);  // Auto-start server on DoStart()
        
        // Server control
        bool StartServer();
        void StopServer();
        bool IsServerRunning() const;
        
        // Connection management
        int GetConnectionCount() const;
        
        // Broadcasting (to all connected editors)
        void BroadcastCoreMetrics(const CoreMetrics& metrics);
        void BroadcastData(const StringCRC& dataType, const Json::Value& payload);
        void BroadcastEvent(const StringCRC& eventType, const Json::Value& payload);
        
        // Notify subscribers of data type
        void NotifySubscribers(const StringCRC& dataType, const Json::Value& payload);
        
        // Register protocol command handlers (for custom protocol commands)
        using ProtocolCommandHandler = std::function<void(const Json::Value& payload, Json::Value& responseOut)>;
        void RegisterProtocolCommand(const StringCRC& commandName, ProtocolCommandHandler handler);
        void UnregisterProtocolCommand(const StringCRC& commandName);
        
    private:
        Dia::WebSocket::Server* mServer;
        uint16_t mPort;
        float mMetricsBroadcastInterval;
        bool mIsRunning;
    };
    
    struct CoreMetrics {
        float fps;
        float frameTimeMs;
        float memoryUsedMb;
        float memoryAvailableMb;
    };
}
```

**Note:** WebSocket functionality is provided by DiaWebSocket::Server (see @docs/specs/systems/dia/diawebsocket.md)

**StateSerializer (serializes game state to JSON):**
```cpp
namespace Dia::DebugServer {
    class StateSerializer {
    public:
        // ProcessingUnit state
        static Json::Value SerializeProcessingUnitState(const Dia::Application::ProcessingUnit* pu);
        
        // Phase state
        static Json::Value SerializePhaseState(const Dia::Application::Phase* phase);
        
        // Module state
        static Json::Value SerializeModuleState(const Dia::Application::Module* module);
        
        // Core metrics
        static Json::Value SerializeCoreMetrics(const CoreMetrics& metrics);
        
        // Phase transition event
        static Json::Value SerializePhaseTransition(const StringCRC& fromPhase, 
                                                    const StringCRC& toPhase,
                                                    uint64_t timestamp);
        
        // Command response
        static Json::Value SerializeCommandResponse(const StringCRC& command,
                                                    bool success,
                                                    const char* message);
    };
}
```

**SubscriptionManager (tracks editor subscriptions):**
```cpp
namespace Dia::DebugServer {
    struct Subscription {
        int connectionId;
        StringCRC dataType;
        Json::Value filter;  // Optional filter (e.g., { "pu_id": "MainProcessingUnit" })
    };
    
    class SubscriptionManager {
    public:
        void Subscribe(int connectionId, const StringCRC& dataType, const Json::Value& filter);
        void Unsubscribe(int connectionId, const StringCRC& dataType);
        void UnsubscribeAll(int connectionId);
        
        // Get all connection IDs subscribed to a data type
        DynamicArrayC<int, 16> GetSubscribers(const StringCRC& dataType) const;
        
        // Check if connection is subscribed
        bool IsSubscribed(int connectionId, const StringCRC& dataType) const;
        
    private:
        DynamicArrayC<Subscription, 64> mSubscriptions;
        Dia::Core::Mutex mMutex;
    };
}
```

**CommandDispatcher (forwards commands to DiaAPI):**
```cpp
namespace Dia::DebugServer {
    class CommandDispatcher {
    public:
        CommandDispatcher();
        
        // Execute DiaAPI command received from editor
        Json::Value ExecuteDiaAPICommand(const StringCRC& commandName, 
                                         const Json::Value& argsJson);
        
        // Execute protocol command (subscribe, unsubscribe, get_state)
        Json::Value ExecuteProtocolCommand(const StringCRC& commandName,
                                          const Json::Value& payload,
                                          DebugServerModule* server);
        
        // Register custom protocol command handler
        void RegisterProtocolCommand(const StringCRC& commandName,
                                     DebugServerModule::ProtocolCommandHandler handler);
        
    private:
        // Convert JSON to DiaAPI::CommandArgs
        DiaAPI::CommandArgs JsonToCommandArgs(const Json::Value& argsJson);
        
        HashTable<StringCRC, DebugServerModule::ProtocolCommandHandler> mProtocolHandlers;
    };
}
```

## Protocol Definition

Message types, wire format, serialization helpers, and data type constants are defined in the shared **DiaDebugProtocol** system (@docs/specs/systems/dia/diadebugprotocol.md). DiaDebugServer uses these types directly for all message construction and parsing.

DiaDebugServer-specific protocol behavior:

- **Core metrics** broadcast to all connections every 500ms (DDS-006), no subscription needed
- **Phase transitions** pushed as immediate events (DDS-007)
- **DiaAPI commands** forwarded to CommandRegistry and results returned (DDS-008)
- **Protocol commands** (`get_state`, `subscribe`, `unsubscribe`) handled by DebugServerModule directly
- **Data type constants** for DiaApplication subscriptions defined in `DiaApplication/DebugDataTypes.h` per DDP-006

## Dependencies

### Required Systems (from Dia)
- **DiaCore** - Containers (DynamicArrayC, HashTable), StringCRC, JSON parsing (jsoncpp wrapper), logging, Threading (Thread, Mutex, ScopedLock)
- **DiaApplication** - Module base class, MessageBus (for event forwarding), ProcessingUnit introspection, HotReloadManager
- **DiaAPI** - CommandRegistry (execute commands remotely), CommandArgs, ArgumentParser
- **DiaWebSocket** - WebSocket::Server for accepting editor connections (wraps websocketpp)
- **DiaDebugProtocol** - Shared message types, serialization helpers, and data type constants (header-only, @docs/specs/systems/dia/diadebugprotocol.md)

## Non-Responsibilities

What DiaDebugServer explicitly does NOT handle:

- **Editor UI** - That's DiaEditor's job (DiaDebugServer just provides data)
- **Asset Streaming** - Only sends text/JSON, not large binary assets (textures, meshes)
- **Save Games** - Not a save system, just debugging/inspection
- **Authentication** - No security/auth in Phase 5 (localhost only, Debug builds)
- **Game Logic** - Just observes and forwards commands, doesn't implement game features

## Related Systems

| System | Relationship | Interface |
|--------|--------------|-----------|
| DiaAPI | Command provider | DiaDebugServer forwards remote commands to CommandRegistry |
| DiaApplication | Data source | Introspects ProcessingUnit/Phase/Module state for serialization |
| DiaEditor | Consumer | GameConnectionManager connects to DiaDebugServer via WebSocket |
| DiaApplicationEditor | Consumer | Subscribes to "processing_unit_state" for live visualization |
| DiaDebugProtocol | Shared dependency | Provides message types, serialization helpers (header-only) |

## Inherited Binding Decisions

These decisions from parent platform and application specs are binding constraints on DiaDebugServer:

| Source | ID | Decision | Impact on DiaDebugServer |
|--------|----|----------|--------------------------|
| Platform | PD-001 | C++ as primary language | DiaDebugServer core written in C++; protocol uses JSON |
| Platform | PD-002 | Windows as primary platform | Test on Windows first; websocketpp is cross-platform |
| Platform | PD-003 | Visual Studio + MSBuild | Built as .vcxproj in Dia solution |
| Platform | PD-004 | Spec-driven development | This spec approved before implementation |
| Dia | AD-001 | Module system with YAML frontmatter | Document as `dia.dia.diadebugserver.architecture.module.md` |
| Dia | AD-002 | No STL in public APIs | Public methods use `const char*`, DynamicArrayC, not std::string, std::vector |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | All classes in `Dia::DebugServer::` namespace |
| Dia | AD-004 | ProcessingUnit/Phase/Module for apps | DebugServerModule extends Module, uses PU lifecycle |

## System-Specific Decisions

Decisions specific to DiaDebugServer. Binding decisions constrain all features within this system.

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| DDS-001 | Generic system, not DiaApplication-specific | Any Dia app can use it; extensible for future systems (graphics, physics) | Proposed | Yes |
| DDS-002 | Broadcast core metrics always, specialized data on subscription (Q1 - Option C) | Balance between discoverability and bandwidth | Proposed | Yes |
| DDS-003 | Broadcast to all connected editors (Q1 - Option A) | Simpler protocol; editors filter what they need | Proposed | Yes |
| DDS-004 | Always listen in Debug builds, disabled in Release (Q1 - Option C) | Low overhead if no connections; Release builds exclude entirely | Proposed | Yes |
| DDS-005 | JSON over WebSocket (not binary) (Q2) | Human-readable for debugging; flexible schema; standard tooling support | Proposed | Yes |
| DDS-006 | Core metrics broadcast every 500ms (Q2) | Enough for monitoring, low bandwidth | Proposed | Yes |
| DDS-007 | Push phase transitions as immediate events (Q2) | Real-time debugging excitement; no polling needed | Proposed | Yes |
| DDS-008 | DiaDebugServer forwards commands to DiaAPI CommandRegistry | Reuses existing command infrastructure; consistency with DiaCLI | Proposed | Yes |
| DDS-009 | Use DiaWebSocket::Server (wraps websocketpp) | Reusable abstraction; consistent with Dia patterns; isolates external dependency | Proposed | Yes |
| DDS-010 | JSON serialization for all messages | Human-readable, debuggable, flexible; uses DiaCore's jsoncpp wrapper | Proposed | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced on all features · `No` = guidance only

## Features

Features within the DiaDebugServer system (create with `/spec-feature`):

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| WebSocket Server Management | Start/stop server, handle connections, thread management | @docs/specs/features/dia/diadebugserver/websocket-server-management.md | Approved |
| Core Metrics Broadcasting | Broadcast FPS, memory, frame time every 500ms | @docs/specs/features/dia/diadebugserver/core-metrics-broadcasting.md | Approved |
| Subscription Management | Handle subscribe/unsubscribe requests, track subscriptions per connection | @docs/specs/features/dia/diadebugserver/subscription-management.md | Approved |
| State Serialization | Serialize ProcessingUnit/Phase/Module state to JSON | @docs/specs/features/dia/diadebugserver/state-serialization.md | Approved |
| Event Broadcasting | Forward MessageBus events to subscribed editors | @docs/specs/features/dia/diadebugserver/event-broadcasting.md | Approved |
| DiaAPI Command Gateway | Receive commands via WebSocket, forward to CommandRegistry, return results | @docs/specs/features/dia/diadebugserver/diaapi-command-gateway.md | Approved |
| Protocol Command Handling | Handle subscribe, unsubscribe, get_state commands | @docs/specs/features/dia/diadebugserver/protocol-command-handling.md | Approved |
| Query Registry | Structured remote query dispatch — domain modules register JSON-in/JSON-out handlers; migrates protocol commands | @docs/specs/features/dia/diadebugserver/query-registry.md | Approved |
| Server Self-Monitoring | Track game/editor performance impact, server health metrics, debugging | @docs/specs/features/dia/diadebugserver/server-self-monitoring.md | Approved |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Generic or DiaApplication-specific? | Generic (DDS-001) - any Dia app can use it; extensible for future systems |
| 2 | Broadcasting | What to broadcast by default? | Core metrics always (DDS-002); specialized data on subscription |
| 3 | Multi-Connection | Separate streams or broadcast? | Broadcast to all (DDS-003) - simpler, editors filter |
| 4 | Opt-In | Enabled by default? | Always in Debug (DDS-004), excluded from Release builds |
| 5 | Protocol | JSON or binary? | JSON (DDS-005) - human-readable, debuggable, flexible |
| 6 | Frequency | Core metrics broadcast rate? | Every 500ms (DDS-006) - 2 Hz, low bandwidth |
| 7 | Events | Push or poll? | Push immediately (DDS-007) - phase transitions as events |
| 8 | Commands | Own system or integrate DiaAPI? | Integrate DiaAPI (DDS-008) - reuse CommandRegistry, consistency |
| 9 | Threading | std:: or DiaCore? | DiaCore (DDS-009) - Thread, Mutex, ScopedLock for consistency |
| 10 | WebSocket Library | Which library? | websocketpp (DDS-010) - header-only, standalone, easy integration |

## Status

`Done`

## Notes

**Integration with DiaAPI:**
DiaDebugServer acts as a network gateway to DiaAPI. Any command registered with DiaAPI's CommandRegistry becomes remotely executable:
```cpp
// Module registers command locally
DiaAPI::CommandRegistry::Instance().RegisterCommand({
    .name = StringCRC("validate_manifest"),
    .callback = [](const CommandArgs& args) { /* ... */ return 0; },
    .description = "Validate manifest file"
});

// Automatically available remotely via DiaDebugServer
// Editor sends: { "type": "command", "command": "validate_manifest", "payload": {...} }
// DiaDebugServer forwards to CommandRegistry → executes → returns result
```

**Thread Safety:**
DiaDebugServer runs on a separate thread (server thread). All communication with the main game thread uses DiaCore::Mutex and message queues:
- WebSocket messages queued → processed in DoUpdate() (main thread)
- Broadcast requests queued → sent from server thread
- Subscription changes protected by mutex

**Debug vs Release:**
```cpp
#ifdef DIA_DEBUG
    // Include DiaDebugServer module in Debug builds
    mProcessingUnit->AddModule(new Dia::DebugServer::DebugServerModule(mProcessingUnit, "DebugServer"));
#endif
// Release builds exclude DebugServerModule entirely (zero overhead)
```

**Performance Impact:**
- Minimal when no editors connected (server just listens)
- Core metrics serialization: ~100μs per frame
- State serialization (on subscription): ~1-5ms depending on complexity
- Network I/O on separate thread (non-blocking)
