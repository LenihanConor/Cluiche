# Feature Spec: Type Discovery

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Type Discovery** | (this document) |

## Problem Statement

Discovers available ProcessingUnit, Phase, and Module types by querying ApplicationTypeRegistry in connected game (if running) or falling back to bundled static type list, populating dropdowns for adding new components.

## Acceptance Criteria

- [x] Query ApplicationTypeRegistry when game connected
- [x] Fallback to bundled static type list when offline
- [x] Populate dropdowns with discovered types when adding PU/Phase/Module
- [x] Show type descriptions in dropdown (tooltip or subtitle)
- [x] Cache discovered types for performance
- [x] Refresh types on game reconnect
- [x] Display "online" vs "offline" indicator for type source
- [x] Filter types by category (e.g., only show Phase types when adding phase)
- [x] Handle custom game-specific types not in static list
- [x] Warn if type not found in registry during validation

## Design

### Type Discovery Strategy

**Hybrid Approach (Decision 50):**
1. **Connected to Game**: Query ApplicationTypeRegistry via DiaDebugServer
2. **Offline**: Use bundled `known_types.json` with common Dia types

### C++ Type Query Handler

**DiaApplicationEditor::RefreshAvailableTypes:**
```cpp
void DiaApplicationEditor::RefreshAvailableTypes() {
    if (mPluginData->isConnectedToGame) {
        // Query game's ApplicationTypeRegistry via WebSocket
        Json::Value request;
        request["type"] = "query";
        request["query_type"] = "application_types";
        
        mEditorModel->GetGameConnection()->SendMessage(request);
        
        // Response handled in HandleTypeQueryResponse()
    } else {
        // Load from bundled static list
        LoadStaticTypeList();
    }
}
```

**DiaApplicationEditor::HandleTypeQueryResponse:**
```cpp
void DiaApplicationEditor::HandleTypeQueryResponse(const Json::Value& response) {
    if (!response["success"].asBool()) {
        DIA_LOG_WARNING("Type query failed, falling back to static list");
        LoadStaticTypeList();
        return;
    }
    
    // Parse available types
    const Json::Value& types = response["types"];
    
    mPluginData->availableProcessingUnitTypes.Clear();
    mPluginData->availablePhaseTypes.Clear();
    mPluginData->availableModuleTypes.Clear();
    
    for (const auto& typeInfo : types["processing_units"]) {
        StringCRC typeId(typeInfo["type"].asCString());
        mPluginData->availableProcessingUnitTypes.Add(typeId);
    }
    
    for (const auto& typeInfo : types["phases"]) {
        StringCRC typeId(typeInfo["type"].asCString());
        mPluginData->availablePhaseTypes.Add(typeId);
    }
    
    for (const auto& typeInfo : types["modules"]) {
        StringCRC typeId(typeInfo["type"].asCString());
        mPluginData->availableModuleTypes.Add(typeId);
    }
    
    // Notify UI
    mEditorModel->NotifyObservers(StringCRC("types_refreshed"));
    
    DIA_LOG("Discovered %d module types from running game",
            mPluginData->availableModuleTypes.GetSize());
}
```

**DiaApplicationEditor::LoadStaticTypeList:**
```cpp
void DiaApplicationEditor::LoadStaticTypeList() {
    // Load known_types.json from plugin assets
    Dia::Core::FilePath typesPath("Dia/DiaApplicationEditor/Assets/known_types.json");
    
    Json::Value typesJson;
    if (!LoadJsonFile(typesPath.GetPath(), typesJson)) {
        DIA_LOG_ERROR("Failed to load static type list");
        return;
    }
    
    // Parse and populate type arrays
    // ... (similar to HandleTypeQueryResponse)
    
    mEditorModel->NotifyObservers(StringCRC("types_refreshed"));
    
    DIA_LOG("Loaded static type list (offline mode)");
}
```

### Static Type List

**known_types.json:**
```json
{
    "processing_units": [
        { "type": "ProcessingUnit", "description": "Generic processing unit" }
    ],
    "phases": [
        { "type": "InitPhase", "description": "Initialization phase" },
        { "type": "UpdatePhase", "description": "Update logic phase" },
        { "type": "RenderPhase", "description": "Rendering phase" },
        { "type": "ShutdownPhase", "description": "Cleanup phase" }
    ],
    "modules": [
        { "type": "DebugServerModule", "description": "WebSocket debug server" },
        { "type": "RenderModule", "description": "Graphics rendering" },
        { "type": "InputModule", "description": "Input handling" },
        { "type": "PhysicsModule", "description": "Physics simulation" }
    ]
}
```

### React Type Selector Component

**TypeSelector.tsx:**
```typescript
import React, { useState, useEffect } from 'react';

interface TypeSelectorProps {
    category: 'processing_unit' | 'phase' | 'module';
    onSelect: (typeId: string) => void;
    onCancel: () => void;
}

export const TypeSelector: React.FC<TypeSelectorProps> = ({
    category,
    onSelect,
    onCancel
}) => {
    const [types, setTypes] = useState<TypeInfo[]>([]);
    const [isOnline, setIsOnline] = useState(false);
    const [searchTerm, setSearchTerm] = useState('');
    
    useEffect(() => {
        // Load types from C++
        const data = window.DiaEditor.getPluginData('DiaApplicationEditor');
        
        let typeList = [];
        if (category === 'processing_unit') {
            typeList = data.availableProcessingUnitTypes;
        } else if (category === 'phase') {
            typeList = data.availablePhaseTypes;
        } else if (category === 'module') {
            typeList = data.availableModuleTypes;
        }
        
        setTypes(typeList);
        setIsOnline(data.isConnectedToGame);
    }, [category]);
    
    const filteredTypes = types.filter(type =>
        type.type.toLowerCase().includes(searchTerm.toLowerCase()) ||
        type.description.toLowerCase().includes(searchTerm.toLowerCase())
    );
    
    return (
        <div className="type-selector">
            <div className="type-selector-header">
                <h3>Select {category.replace('_', ' ')} Type</h3>
                <span className={`source-indicator ${isOnline ? 'online' : 'offline'}`}>
                    {isOnline ? '🟢 Live Game Types' : '🔵 Bundled Types'}
                </span>
            </div>
            
            <input
                type="text"
                placeholder="Search types..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="type-search"
            />
            
            <div className="type-list">
                {filteredTypes.map(type => (
                    <div
                        key={type.type}
                        className="type-item"
                        onClick={() => onSelect(type.type)}
                    >
                        <div className="type-name">{type.type}</div>
                        <div className="type-description">{type.description}</div>
                    </div>
                ))}
            </div>
            
            <div className="type-selector-footer">
                <button onClick={onCancel}>Cancel</button>
            </div>
        </div>
    );
};
```

### Game-Side Type Query Handler

**DiaDebugServer integration:**
```cpp
// In DiaDebugServer::HandleMessage
if (messageType == "query" && queryType == "application_types") {
    Json::Value response;
    response["success"] = true;
    response["types"] = SerializeApplicationTypes();
    
    SendMessage(connectionId, response);
}

Json::Value DiaDebugServer::SerializeApplicationTypes() {
    Json::Value types;
    
    // Query ApplicationTypeRegistry
    const auto& registry = Dia::Application::ApplicationTypeRegistry::Instance();
    
    // Processing Units
    Json::Value puTypes(Json::arrayValue);
    for (const auto& typeId : registry.GetRegisteredProcessingUnitTypes()) {
        Json::Value typeInfo;
        typeInfo["type"] = typeId.GetString();
        typeInfo["description"] = registry.GetTypeDescription(typeId);
        puTypes.append(typeInfo);
    }
    types["processing_units"] = puTypes;
    
    // Phases
    Json::Value phaseTypes(Json::arrayValue);
    for (const auto& typeId : registry.GetRegisteredPhaseTypes()) {
        Json::Value typeInfo;
        typeInfo["type"] = typeId.GetString();
        typeInfo["description"] = registry.GetTypeDescription(typeId);
        phaseTypes.append(typeInfo);
    }
    types["phases"] = phaseTypes;
    
    // Modules
    Json::Value moduleTypes(Json::arrayValue);
    for (const auto& typeId : registry.GetRegisteredModuleTypes()) {
        Json::Value typeInfo;
        typeInfo["type"] = typeId.GetString();
        typeInfo["description"] = registry.GetTypeDescription(typeId);
        moduleTypes.append(typeInfo);
    }
    types["modules"] = moduleTypes;
    
    return types;
}
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Type discovery and caching
- `Dia/DiaApplicationEditor/UI/TypeSelector.tsx` - Type selection dialog
- `Dia/DiaApplicationEditor/Assets/known_types.json` - Bundled static type list
- `Dia/DiaDebugServer/DiaDebugServer.cpp` - Type query response handler

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Type IDs use StringCRC |
| DiaApplicationEditor | DAED-004 | Query ApplicationTypeRegistry for available types | ✅ **Compliant** - Primary strategy |

**Decision 50 Compliance:**
- **Hybrid registry query + static fallback** - ✅ Implemented: try game registry first, fallback to bundled known_types.json

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Query live game or use static list? | Decision 50 | ✅ Hybrid - query game if connected, fallback to static |
| 2 | Where to store static list? | Decision 50 | ✅ Bundled in plugin assets: known_types.json |
| 3 | Refresh types on reconnect? | Decision 50 | ✅ Yes - auto-refresh when game connects |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Strategy | Why hybrid approach? | ✅ Decision 50 - game has custom types; static list covers common Dia types |
| 2 | Performance | Cache types? | ✅ Yes - cached in mPluginData; refreshed on game reconnect |
| 3 | UX | Show source (online vs offline)? | ✅ Yes - indicator in type selector dialog |
| 4 | Validation | Handle unknown types? | ✅ Validation warns if type not in registry; user can proceed (game may have it) |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
