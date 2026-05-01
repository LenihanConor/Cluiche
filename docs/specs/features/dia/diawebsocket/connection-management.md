# Feature Spec: Connection Management

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaWebSocket | @docs/specs/systems/dia/diawebsocket.md |
| Feature | **Connection Management** | (this document) |

## Problem Statement

Provides server-side connection tracking, lifecycle management, and enforcement of connection limits so that consumers (DiaDebugServer, DiaEditor) can identify, query, and control individual client connections.

## Acceptance Criteria

- [ ] Server assigns unique incrementing integer IDs to each connection on open
- [ ] Server maintains a map of connection ID → websocketpp handle for send/close operations
- [ ] Server maintains a reverse map of websocketpp handle pointer → connection ID for O(1) handler lookups
- [ ] GetConnectionCount() returns current number of active connections (thread-safe)
- [ ] GetActiveConnectionIds() returns DynamicArrayC<int, 16> of all active connection IDs (thread-safe)
- [ ] CloseConnection(connectionId) initiates graceful close of a specific connection
- [ ] CloseConnection on invalid/stale connection ID is a no-op (no crash, no error)
- [ ] SetMaxConnections(int max) configures the connection limit (must be called before Start(); asserts if max > 16)
- [ ] When max connections reached, new connections are rejected with websocketpp close status "try again later"
- [ ] Rejected connections fire the error callback with a descriptive message
- [ ] Connection open/close events are queued to the incoming queue and delivered via ConnectionCallback during Update()
- [ ] Server Stop() closes all active connections with "going away" status and clears all tracking state
- [ ] Connection IDs are never reused within a single server session (monotonically incrementing)

## Design

### Connection Tracking Data Structures

```cpp
// Inside Server::Impl (private)
std::map<int, Internal::ConnectionHdl> mConnectionsById;   // ID → handle (for send/close)
std::map<void*, int> mConnectionIdByPtr;                    // handle raw ptr → ID (for handler lookups)
int mNextConnectionId = 1;                                  // Monotonically incrementing
Dia::Core::Mutex mConnectionsMutex;                         // Protects all connection state
```

### Connection ID Assignment

On `OnOpen`, the server locks `mConnectionsMutex`, checks the max connections limit, assigns the next ID, and populates both maps. The connection ID plus `connected=true` is queued as a `QueuedEvent` for main-thread delivery.

### Connection Lookup

`FindConnectionId(ConnectionHdl hdl)` extracts the raw pointer via `hdl.lock().get()` and looks up the reverse map. This avoids O(n) iteration over the connections-by-ID map inside every message handler.

### Max Connections Enforcement

```cpp
void Server::Impl::OnOpen(Internal::ConnectionHdl hdl) {
    ScopedLock lock(mConnectionsMutex);
    if (static_cast<int>(mConnectionsById.size()) >= mMaxConnections) {
        mServer.close(hdl, websocketpp::close::status::try_again_later, "Server full");
        QueueError(-1, "Connection rejected: server full");
        return;
    }
    // ... assign ID, track connection
}
```

### CloseConnection

```cpp
void Server::CloseConnection(int connectionId) {
    ScopedLock lock(mConnectionsMutex);
    auto it = mConnectionsById.find(connectionId);
    if (it != mConnectionsById.end()) {
        mServer.close(it->second, websocketpp::close::status::normal, "Connection closed by server");
    }
    // Invalid connectionId is silently ignored
}
```

The actual removal from tracking maps happens in `OnClose` when websocketpp confirms the close handshake.

### Connection Cleanup on Stop

`Server::Stop()` iterates all tracked connections, sends "going away" close frames, then clears both maps. The worker thread is joined after connections are closed.

### Public Query API

```cpp
int Server::GetConnectionCount() const {
    ScopedLock lock(mConnectionsMutex);
    return static_cast<int>(mConnectionsById.size());
}

DynamicArrayC<int, 16> Server::GetActiveConnectionIds() const {
    ScopedLock lock(mConnectionsMutex);
    DynamicArrayC<int, 16> ids;
    for (const auto& pair : mConnectionsById) {
        if (!ids.IsFull()) ids.Add(pair.first);
    }
    return ids;
}
```

## Implementation Files

- `Dia/DiaWebSocket/Server.h` - Public API (GetConnectionCount, GetActiveConnectionIds, CloseConnection, SetMaxConnections)
- `Dia/DiaWebSocket/Server.cpp` - Connection tracking in Server::Impl, OnOpen/OnClose handlers

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Connection IDs are `int` (not entity IDs); StringCRC not applicable |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Public API returns `DynamicArrayC<int, 16>`, not `std::vector`; `std::map` only used internally in Impl |
| Platform | PD-006 | Visual Studio project files are source of truth | ✅ **Compliant** - No new files; functionality in existing Server.h/Server.cpp |
| Dia | AD-001 | Module system with YAML frontmatter | ✅ **Compliant** - DiaWebSocket module doc already exists |
| Dia | AD-002 | No STL containers in public APIs | ✅ **Compliant** - Uses `DynamicArrayC<int, 16>` for connection ID list |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All in `Dia::WebSocket::` namespace |
| DiaWebSocket | DWS-001 | Wrap websocketpp, don't expose in public API | ✅ **Compliant** - Connection handles hidden behind Pimpl; public API uses `int` connection IDs |
| DiaWebSocket | DWS-002 | Server runs on separate thread | ✅ **Compliant** - Connection tracking protected by mutex; accessed from both threads |
| DiaWebSocket | DWS-003 | Update() pattern for callbacks | ✅ **Compliant** - Connection events queued and delivered during Update() |
| DiaWebSocket | DWS-005 | Message queue between threads | ✅ **Compliant** - Connection events use same QueuedEvent system as messages |
| DiaWebSocket | DWS-007 | websocketpp standalone mode (no Boost) | ✅ **Compliant** - No new dependencies |

**All binding decisions: COMPLIANT. No conflicts detected.**

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Connection IDs | What happens if mNextConnectionId wraps at INT_MAX? | With monotonic increment and no reuse, would need ~2 billion connections per server session to wrap — not a practical concern | ✅ Not a practical concern — ~2 billion connections per session is unreachable for localhost debugging use cases |
| 2 | Query Thread Safety | Can GetConnectionCount/GetActiveConnectionIds be called from any thread? | Yes — both lock mConnectionsMutex, safe from any thread | ✅ Yes, callable from any thread — both lock mConnectionsMutex |
| 3 | Max Connections | What's the practical upper bound for DynamicArrayC<int, 16> in GetActiveConnectionIds? | 16 slots matches the default max connections; if SetMaxConnections(>16), some IDs won't be returned | ✅ Keep it simple — 16 capacity matches default max. Assert in SetMaxConnections if value exceeds 16 |
| 4 | Close Handshake | What if CloseConnection is called but websocketpp close handshake takes time? | Connection remains in tracking maps until OnClose fires; GetConnectionCount still counts it | ✅ Accepted — connection stays tracked until OnClose confirms the handshake |
| 5 | Concurrency | Is it safe to call CloseConnection from inside a ConnectionCallback? | Yes — callback fires on main thread during Update(); CloseConnection queues the close for the worker thread | ✅ Yes — simple and safe; main thread queues close for worker thread, no deadlock risk |

## Status

`Done` - Implemented and tested
