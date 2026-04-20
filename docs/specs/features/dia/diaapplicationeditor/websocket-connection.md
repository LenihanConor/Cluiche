# Feature Spec: WebSocket Connection

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **WebSocket Connection** | (this document) |

## Problem Statement

Establishes WebSocket connection to running game via shared GameConnectionManager (owned by EditorApplication), auto-connecting when editor opens if game is running, enabling live debugging and real-time state visualization.

## Acceptance Criteria

- [x] Use shared GameConnectionManager singleton from EditorApplication
- [x] Auto-connect to localhost:8080 on plugin load (if game running)
- [x] Display connection status indicator in toolbar (Connected/Disconnected)
- [x] Manual Connect/Disconnect buttons for user control
- [x] Subscribe to DiaDebugServer events: processing_unit_state, phase_transition
- [x] Handle connection loss gracefully (show reconnect button)
- [x] Auto-reconnect with exponential backoff (1s, 2s, 4s, max 10s)
- [x] Display connection info: address, port, uptime
- [x] Share connection across all editor plugins (not per-plugin)
- [x] Handle game restart (detect disconnect, attempt reconnect)

## Design

### GameConnectionManager (Shared Singleton)

**Architecture Note (Decision 55 clarification):**
- GameConnectionManager is owned by EditorApplication (not per-plugin)
- All plugins share the same WebSocket connection
- Plugins subscribe to specific events they care about

**EditorApplication::GameConnectionManager:**
```cpp
namespace Dia::Editor {
    class GameConnectionManager {
    public:
        static GameConnectionManager& Instance();
        
        // Connection lifecycle
        bool Connect(const char* address, uint16_t port);
        void Disconnect();
        bool IsConnected() const;
        
        // Event subscription
        void Subscribe(const StringCRC& eventType, IConnectionObserver* observer);
        void Unsubscribe(const StringCRC& eventType, IConnectionObserver* observer);
        
        // Send commands
        void SendCommand(const Json::Value& command);
        
        // Connection info
        const char* GetAddress() const;
        uint16_t GetPort() const;
        float GetUptime() const;
        
    private:
        GameConnectionManager();
        
        Dia::WebSocket::Client* mClient;
        Dia::Core::String mAddress;
        uint16_t mPort;
        bool mIsConnected;
        float mConnectedTime;
        
        // Observers
        struct Subscription {
            StringCRC eventType;
            DynamicArrayC<IConnectionObserver*, 16> observers;
        };
        DynamicArrayC<Subscription, 32> mSubscriptions;
        
        // Message handler
        void HandleMessage(const Dia::WebSocket::Message& msg);
        void NotifyObservers(const StringCRC& eventType, const Json::Value& data);
    };
    
    class IConnectionObserver {
    public:
        virtual ~IConnectionObserver() = default;
        virtual void OnGameEvent(const StringCRC& eventType, const Json::Value& data) = 0;
    };
}
```

### DiaApplicationEditor Integration

**DiaApplicationEditor::OnLoad:**
```cpp
void DiaApplicationEditor::OnLoad(Dia::Editor::EditorModel* model) {
    mEditorModel = model;
    mPluginData = new ManifestEditorData();
    
    // Subscribe to game connection events via shared GameConnectionManager
    auto& connMgr = Dia::Editor::GameConnectionManager::Instance();
    
    connMgr.Subscribe(StringCRC("processing_unit_state"), this);
    connMgr.Subscribe(StringCRC("phase_transition"), this);
    connMgr.Subscribe(StringCRC("hot_reload_response"), this);
    
    // Auto-connect if game is running
    if (!connMgr.IsConnected()) {
        bool connected = connMgr.Connect("localhost", 8080);
        if (connected) {
            DIA_LOG("Auto-connected to game");
            mPluginData->isConnectedToGame = true;
            mEditorModel->NotifyObservers(StringCRC("game_connected"));
        } else {
            DIA_LOG("Game not running (auto-connect failed)");
        }
    } else {
        // Already connected (shared connection)
        mPluginData->isConnectedToGame = true;
        DIA_LOG("Using existing game connection");
    }
}

void DiaApplicationEditor::OnUnload() {
    // Unsubscribe from game events
    auto& connMgr = Dia::Editor::GameConnectionManager::Instance();
    connMgr.Unsubscribe(StringCRC("processing_unit_state"), this);
    connMgr.Unsubscribe(StringCRC("phase_transition"), this);
    connMgr.Unsubscribe(StringCRC("hot_reload_response"), this);
    
    // Don't disconnect - other plugins may be using connection
    
    delete mPluginData;
}

// IConnectionObserver implementation
void DiaApplicationEditor::OnGameEvent(const StringCRC& eventType, const Json::Value& data) {
    if (eventType == StringCRC("processing_unit_state")) {
        HandleProcessingUnitState(data);
    }
    else if (eventType == StringCRC("phase_transition")) {
        HandlePhaseTransition(data);
    }
    else if (eventType == StringCRC("hot_reload_response")) {
        HandleHotReloadResponse(data);
    }
}
```

### Manual Connect/Disconnect

**DiaApplicationEditor::ConnectToGame:**
```cpp
void DiaApplicationEditor::ConnectToGame(const char* address, uint16_t port) {
    auto& connMgr = Dia::Editor::GameConnectionManager::Instance();
    
    if (connMgr.IsConnected()) {
        DIA_LOG_WARNING("Already connected to game");
        return;
    }
    
    bool connected = connMgr.Connect(address, port);
    
    if (connected) {
        mPluginData->isConnectedToGame = true;
        mEditorModel->NotifyObservers(StringCRC("game_connected"));
        
        // Refresh available types from game registry
        RefreshAvailableTypes();
        
        DIA_LOG("Connected to game: %s:%d", address, port);
    } else {
        mEditorModel->NotifyObservers(StringCRC("game_connection_failed"));
        DIA_LOG_ERROR("Failed to connect to game: %s:%d", address, port);
    }
}

void DiaApplicationEditor::DisconnectFromGame() {
    auto& connMgr = Dia::Editor::GameConnectionManager::Instance();
    
    if (!connMgr.IsConnected()) {
        DIA_LOG_WARNING("Not connected to game");
        return;
    }
    
    // Note: This disconnects for ALL plugins using shared connection
    // Show warning if other plugins are active
    
    connMgr.Disconnect();
    mPluginData->isConnectedToGame = false;
    mEditorModel->NotifyObservers(StringCRC("game_disconnected"));
    
    DIA_LOG("Disconnected from game");
}
```

### React Connection Status Component

**ConnectionStatus.tsx:**
```typescript
import React, { useState, useEffect } from 'react';

export const ConnectionStatus: React.FC = () => {
    const [isConnected, setIsConnected] = useState(false);
    const [connectionInfo, setConnectionInfo] = useState<any>(null);
    
    useEffect(() => {
        const handleConnected = () => {
            setIsConnected(true);
            
            const info = window.DiaEditor.getPluginData('DiaApplicationEditor').connectionInfo;
            setConnectionInfo(info);
        };
        
        const handleDisconnected = () => {
            setIsConnected(false);
            setConnectionInfo(null);
        };
        
        window.DiaEditor.subscribe('game_connected', handleConnected);
        window.DiaEditor.subscribe('game_disconnected', handleDisconnected);
        
        // Check initial state
        const data = window.DiaEditor.getPluginData('DiaApplicationEditor');
        setIsConnected(data.isConnectedToGame);
        
        return () => {
            window.DiaEditor.unsubscribe('game_connected', handleConnected);
            window.DiaEditor.unsubscribe('game_disconnected', handleDisconnected);
        };
    }, []);
    
    const handleConnect = () => {
        window.DiaEditor.executeCommand('diaapp-editor', 'connect_to_game', {
            address: 'localhost',
            port: 8080,
        });
    };
    
    const handleDisconnect = () => {
        window.DiaEditor.executeCommand('diaapp-editor', 'disconnect_from_game', {});
    };
    
    return (
        <div className="connection-status">
            <div className={`status-indicator ${isConnected ? 'connected' : 'disconnected'}`}>
                {isConnected ? '🟢 Connected' : '🔴 Disconnected'}
            </div>
            
            {isConnected ? (
                <>
                    <span className="connection-info">
                        {connectionInfo?.address}:{connectionInfo?.port} 
                        (uptime: {Math.floor(connectionInfo?.uptime || 0)}s)
                    </span>
                    <button onClick={handleDisconnect}>Disconnect</button>
                </>
            ) : (
                <button onClick={handleConnect}>Connect to Game</button>
            )}
        </div>
    );
};
```

### Auto-Reconnect Logic

**GameConnectionManager::Update (called every frame):**
```cpp
void GameConnectionManager::Update() {
    if (!mIsConnected && mShouldAutoReconnect) {
        float currentTime = Dia::Core::Time::GetTime();
        
        if (currentTime - mLastReconnectAttempt > mReconnectDelay) {
            DIA_LOG("Attempting reconnect...");
            
            bool connected = Connect(mAddress.GetData(), mPort);
            
            if (connected) {
                mShouldAutoReconnect = false;
                mReconnectDelay = 1.0f;  // Reset delay
            } else {
                // Exponential backoff: 1s, 2s, 4s, 8s, max 10s
                mReconnectDelay = std::min(mReconnectDelay * 2.0f, 10.0f);
            }
            
            mLastReconnectAttempt = currentTime;
        }
    }
}
```

## Implementation Files

- `Dia/DiaEditor/GameConnectionManager.h` - Shared connection manager interface
- `Dia/DiaEditor/GameConnectionManager.cpp` - Connection lifecycle and event routing
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Plugin subscription and event handling
- `Dia/DiaApplicationEditor/UI/ConnectionStatus.tsx` - Connection status UI component

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Event types use StringCRC |
| DiaEditor | SED-008 | EditorModel uses Observer pattern | ✅ **Compliant** - Plugins observe GameConnectionManager events |

**Decision 55 Compliance:**
- **Auto-connect via shared GameConnectionManager** - ✅ Implemented as EditorApplication singleton, all plugins share connection

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Per-plugin connection or shared? | Decision 55 | ✅ Shared - GameConnectionManager singleton owned by EditorApplication |
| 2 | Auto-connect on startup? | Decision 55 | ✅ Yes - attempts localhost:8080 if game running |
| 3 | Auto-reconnect on disconnect? | Decision 55 | ✅ Yes - exponential backoff (1s, 2s, 4s, max 10s) |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Why shared connection? | ✅ Decision 55 clarification - efficient; game broadcasts to all plugins; avoids multiple connections |
| 2 | Auto-Connect | Always auto-connect? | ✅ Yes - seamless UX; fails silently if game not running |
| 3 | Reconnect | Exponential backoff? | ✅ Yes - prevents spam; max 10s delay |
| 4 | UX | Manual connect button needed? | ✅ Yes - allows retry after manual disconnect; shows connection info |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
