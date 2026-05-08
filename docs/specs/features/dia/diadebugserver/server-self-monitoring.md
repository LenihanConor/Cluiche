# Feature Spec: Server Self-Monitoring

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaDebugServer | @docs/specs/systems/dia/diadebugserver.md |
| Feature | **Server Self-Monitoring** | (this document) |

## Problem Statement

Tracks DiaDebugServer's performance impact on both game and editor, enabling humans and AI to diagnose stability and performance issues with the debug server itself.

## Acceptance Criteria

- [x] Track game-side performance impact (overhead per frame)
- [x] Track editor-side impact (bandwidth, queue sizes, dropped messages)
- [x] Track server health metrics (connections, uptime, message counts)
- [x] Expose get_server_stats protocol command
- [x] Include debug_server_overhead_ms in core_metrics broadcasts
- [x] Log warnings when thresholds exceeded (Phase 6+)
- [x] Editor tracks message_latency_ms via ping/pong (optional)

## Design

### ServerStats Structure

**C++ Struct:**
```cpp
namespace Dia::DebugServer {
    struct ServerStats {
        // Game-side performance impact
        float debugServerOverheadMs;    // Total time in DoUpdate + broadcasts per frame
        float serializationTimeMs;      // Time converting game state to JSON
        float broadcastTimeMs;          // Time in WebSocket send operations
        int subscriptionCount;          // Active data subscriptions (affects broadcast cost)
        
        // Editor-side impact metrics
        float bytesSentPerSec;          // Outgoing bandwidth usage
        int messageQueueSize;           // Current outgoing message queue depth
        int messagesDropped;            // Total dropped due to full queues
        int averageMessageSizeBytes;    // Average payload size
        
        // Server health metrics
        int connectionCount;            // Active WebSocket connections
        int messagesSentTotal;          // Total messages sent since start
        int messagesReceivedTotal;      // Total messages received since start
        int uptimeSeconds;              // Server uptime
    };
}
```

### Tracking Game-Side Performance

**DebugServerModule::DoUpdate:**
```cpp
void DebugServerModule::DoUpdate() {
    if (!mServer || !mServer->IsRunning()) return;
    
    // Start timing
    uint64_t startTime = GetHighResolutionTime();
    
    // Process WebSocket messages
    mServer->Update();
    
    // Broadcast core metrics
    mMetricsTimer += GetDeltaTime();
    if (mMetricsTimer >= mMetricsBroadcastInterval) {
        uint64_t serializeStart = GetHighResolutionTime();
        
        // Gather and serialize metrics
        CoreMetrics metrics = GatherCoreMetrics();
        Json::Value json = StateSerializer::SerializeCoreMetrics(metrics);
        
        mStats.serializationTimeMs = GetElapsedMs(serializeStart);
        
        uint64_t broadcastStart = GetHighResolutionTime();
        
        // Broadcast to all connections
        if (GetConnectionCount() > 0) {
            std::string jsonStr = Json::FastWriter().write(json);
            mServer->BroadcastText(jsonStr.c_str());
            
            // Track bandwidth
            mStats.bytesSentPerSec = jsonStr.size() / mMetricsBroadcastInterval;
            mStats.averageMessageSizeBytes = jsonStr.size();
        }
        
        mStats.broadcastTimeMs = GetElapsedMs(broadcastStart);
        mMetricsTimer = 0.0f;
    }
    
    // Calculate total overhead
    mStats.debugServerOverheadMs = GetElapsedMs(startTime);
    
    // Warn if overhead too high (Phase 6+)
    if (mStats.debugServerOverheadMs > 2.0f) {
        DIA_LOG_WARNING("DebugServer: High overhead %.2fms (>2ms)", mStats.debugServerOverheadMs);
    }
}
```

### Including Overhead in Core Metrics

**Updated Core Metrics JSON:**
```json
{
  "type": "core_metrics",
  "timestamp": 1234567890,
  "payload": {
    "fps": 60.0,
    "frame_time_ms": 16.67,
    "memory_used_mb": 512,
    "memory_available_mb": 2048,
    "debug_server_overhead_ms": 0.3
  }
}
```

### Tracking Editor-Side Impact

**WebSocket Send with Queue Monitoring:**
```cpp
void DebugServerModule::BroadcastData(const StringCRC& dataType, const Json::Value& payload) {
    std::string jsonStr = BuildMessage(dataType, payload);
    
    // Check queue size before sending
    int queueSize = mServer->GetOutgoingQueueSize();
    mStats.messageQueueSize = queueSize;
    
    if (queueSize > 100) {
        DIA_LOG_WARNING("DebugServer: High queue size %d (>100)", queueSize);
    }
    
    // Attempt send
    bool success = mServer->BroadcastText(jsonStr.c_str());
    
    if (!success) {
        mStats.messagesDropped++;
        DIA_LOG_ERROR("DebugServer: Message dropped (queue full)");
    } else {
        mStats.messagesSentTotal++;
    }
}
```

### get_server_stats Protocol Command

**Command Handler:**
```cpp
void DebugServerModule::HandleGetServerStats(int connId, const Json::Value& payload) {
    Json::Value response;
    response["type"] = "command_response";
    response["command"] = "get_server_stats";
    response["success"] = true;
    
    // Serialize stats
    Json::Value statsJson;
    
    // Game-side impact
    statsJson["game_side"]["debug_server_overhead_ms"] = mStats.debugServerOverheadMs;
    statsJson["game_side"]["serialization_time_ms"] = mStats.serializationTimeMs;
    statsJson["game_side"]["broadcast_time_ms"] = mStats.broadcastTimeMs;
    statsJson["game_side"]["subscription_count"] = mStats.subscriptionCount;
    
    // Editor-side impact
    statsJson["editor_side"]["bytes_sent_per_sec"] = mStats.bytesSentPerSec;
    statsJson["editor_side"]["message_queue_size"] = mStats.messageQueueSize;
    statsJson["editor_side"]["messages_dropped"] = mStats.messagesDropped;
    statsJson["editor_side"]["average_message_size_bytes"] = mStats.averageMessageSizeBytes;
    
    // Server health
    statsJson["server"]["connection_count"] = mStats.connectionCount;
    statsJson["server"]["messages_sent_total"] = mStats.messagesSentTotal;
    statsJson["server"]["messages_received_total"] = mStats.messagesReceivedTotal;
    statsJson["server"]["uptime_seconds"] = mStats.uptimeSeconds;
    
    response["payload"] = statsJson;
    
    std::string responseStr = Json::FastWriter().write(response);
    mServer->SendText(connId, responseStr.c_str());
}
```

### Editor-Side Latency Tracking (Optional)

**GameConnectionManager (DiaEditor):**
```cpp
void GameConnectionManager::TrackLatency() {
    // Send ping
    Json::Value ping;
    ping["type"] = "ping";
    ping["timestamp"] = GetTimestamp();
    
    mClient->SendText(Json::FastWriter().write(ping).c_str());
    mPingSentTime = GetTimestamp();
}

void GameConnectionManager::HandlePong(const Json::Value& pong) {
    uint64_t now = GetTimestamp();
    float latencyMs = (now - mPingSentTime) / 1000.0f;
    
    mMessageLatencyMs = latencyMs;
    
    if (latencyMs > 50.0f) {
        DIA_LOG_WARNING("GameConnection: High latency %.2fms (>50ms)", latencyMs);
    }
}
```

**Ping/Pong Protocol:**
```json
// Editor → Game
{
  "type": "ping",
  "timestamp": 1234567890
}

// Game → Editor
{
  "type": "pong",
  "timestamp": 1234567890
}
```

## Implementation Files

- `Dia/DiaDebugServer/DebugServerModule.h` - Add ServerStats struct
- `Dia/DiaDebugServer/DebugServerModule.cpp` - Track metrics in DoUpdate()
- `Dia/DiaDebugServer/ProtocolCommandHandler.cpp` - get_server_stats command
- `Dia/DiaEditor/LiveConnection/GameConnectionManager.cpp` - Ping/pong latency tracking

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaDebugServer | DDS-001 | Generic system | ✅ **Compliant** - Self-monitoring is debug-server specific |
| DiaDebugServer | DDS-010 | JSON serialization | ✅ **Compliant** - get_server_stats returns JSON |

**All binding decisions: COMPLIANT ✅**

## Open Questions

**Resolved:**
- **Decision 17:** Basic self-monitoring with bidirectional performance tracking (game-side overhead + editor-side impact).

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Metrics | Which metrics most critical for diagnosis? | Game overhead + queue sizes + dropped messages | ✅ Comprehensive set (Decision 17) |
| 2 | Thresholds | When to warn about high overhead? | >2ms per frame game-side, >100 queue size editor-side | ✅ Defined thresholds for Phase 6+ |
| 3 | Latency | Should we track editor-side message latency? | Optional - ping/pong mechanism for Phase 6+ | ✅ Optional ping/pong |

## Status

`Done`
