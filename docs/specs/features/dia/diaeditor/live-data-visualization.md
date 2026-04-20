# Feature Spec: Live Data Visualization

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Live Data Visualization** | (this document) |

## Problem Statement

Developers need real-time visibility into running game state including ProcessingUnit hierarchies, performance metrics, and memory usage. Without live visualization, debugging complex multi-threaded applications is difficult. The system must efficiently render updates without impacting editor performance, support pluggable visualizations for extensibility, and filter data by ProcessingUnit to focus on specific subsystems.

## Acceptance Criteria

- [ ] Hybrid rendering: VisJS for hierarchies, recharts for time-series (Decision 38)
- [ ] 10 FPS update throttle (redraw every 100ms) (Decision 39)
- [ ] Read-only visualizations (no inline editing) (Decision 40)
- [ ] Filter by ProcessingUnit dropdown (Decision 41)
- [ ] Pluggable visualization framework with IVisualization interface (Decision 42)
- [ ] Core monitoring base set: ProcessingUnit tree, FPS chart, Memory chart, Frame Time chart (Decision 42)
- [ ] WebSocket subscription to game data via GameConnectionManager
- [ ] Color coding: Running (green), Stopped (gray), Error (red)
- [ ] Toggle individual visualizations on/off
- [ ] Persist visualization layout preferences

## Design

### LiveDataPanel React Component

**UI Component:**
```typescript
// Cluiche/CluicheEditor/UI/src/components/LiveDataPanel.tsx
import React, { useState, useEffect, useRef } from 'react';
import { Network } from 'vis-network';  // VisJS for hierarchy (Decision 38)
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend } from 'recharts';  // recharts for time-series (Decision 38)

export interface IVisualization {
    id: string;
    name: string;
    type: 'hierarchy' | 'timeseries' | 'custom';
    
    // Lifecycle
    initialize(container: HTMLElement): void;
    update(data: any): void;
    destroy(): void;
}

export const LiveDataPanel: React.FC = () => {
    const [visualizations, setVisualizations] = useState<IVisualization[]>([]);
    const [activeVisualizations, setActiveVisualizations] = useState<Set<string>>(new Set());
    const [selectedProcessingUnit, setSelectedProcessingUnit] = useState<string>('all');
    const [processingUnits, setProcessingUnits] = useState<string[]>([]);
    
    // Decision 39: 10 FPS throttle (100ms redraw)
    const lastUpdateTime = useRef<number>(0);
    const updateThrottleMs = 100;
    
    useEffect(() => {
        // Load registered visualizations from C++ (Decision 42: pluggable framework)
        window.CluicheEditor.getVisualizations().then((vizList: IVisualization[]) => {
            setVisualizations(vizList);
            
            // Enable core visualizations by default
            const coreVizIds = vizList
                .filter(viz => ['ProcessingUnitTree', 'FPSChart', 'MemoryChart', 'FrameTimeChart'].includes(viz.id))
                .map(viz => viz.id);
            setActiveVisualizations(new Set(coreVizIds));
        });
        
        // Subscribe to game data via WebSocket
        window.CluicheEditor.subscribeToGameData((data: any) => {
            // Decision 39: Throttle updates to 10 FPS
            const now = Date.now();
            if (now - lastUpdateTime.current >= updateThrottleMs) {
                lastUpdateTime.current = now;
                updateVisualizations(data);
            }
        });
        
        // Load ProcessingUnit list for filter dropdown (Decision 41)
        window.CluicheEditor.getProcessingUnits().then((puList: string[]) => {
            setProcessingUnits(['all', ...puList]);
        });
    }, []);
    
    const updateVisualizations = (data: any) => {
        // Decision 41: Filter by ProcessingUnit
        const filteredData = selectedProcessingUnit === 'all' 
            ? data 
            : filterDataByProcessingUnit(data, selectedProcessingUnit);
        
        // Update each active visualization
        activeVisualizations.forEach(vizId => {
            const viz = visualizations.find(v => v.id === vizId);
            if (viz) {
                viz.update(filteredData);
            }
        });
    };
    
    const filterDataByProcessingUnit = (data: any, puId: string): any => {
        // Filter data to only include selected ProcessingUnit and its children
        if (data.processingUnits) {
            return {
                ...data,
                processingUnits: data.processingUnits.filter((pu: any) => 
                    pu.id === puId || pu.parentId === puId
                )
            };
        }
        return data;
    };
    
    const toggleVisualization = (vizId: string) => {
        setActiveVisualizations(prev => {
            const newSet = new Set(prev);
            if (newSet.has(vizId)) {
                newSet.delete(vizId);
            } else {
                newSet.add(vizId);
            }
            return newSet;
        });
    };
    
    return (
        <div className="live-data-panel">
            {/* Filter bar (Decision 41: ProcessingUnit filter) */}
            <div className="filter-bar">
                <label>
                    Filter by ProcessingUnit:
                    <select 
                        value={selectedProcessingUnit}
                        onChange={(e) => setSelectedProcessingUnit(e.target.value)}
                    >
                        {processingUnits.map(pu => (
                            <option key={pu} value={pu}>{pu}</option>
                        ))}
                    </select>
                </label>
                
                {/* Toggle visualizations */}
                <div className="viz-toggles">
                    {visualizations.map(viz => (
                        <label key={viz.id}>
                            <input 
                                type="checkbox"
                                checked={activeVisualizations.has(viz.id)}
                                onChange={() => toggleVisualization(viz.id)}
                            />
                            {viz.name}
                        </label>
                    ))}
                </div>
            </div>
            
            {/* Visualization grid */}
            <div className="visualization-grid">
                {visualizations.map(viz => (
                    activeVisualizations.has(viz.id) && (
                        <div key={viz.id} className="visualization-container">
                            <h3>{viz.name}</h3>
                            {viz.type === 'hierarchy' && <ProcessingUnitTree vizId={viz.id} />}
                            {viz.type === 'timeseries' && <TimeSeriesChart vizId={viz.id} />}
                            {viz.type === 'custom' && <CustomVisualization vizId={viz.id} />}
                        </div>
                    )
                ))}
            </div>
        </div>
    );
};
```

### Core Visualizations

**ProcessingUnit Tree (VisJS):**
```typescript
// Decision 42: Core monitoring - ProcessingUnit hierarchy
// Decision 38: VisJS for hierarchy visualization
export const ProcessingUnitTree: React.FC<{ vizId: string }> = ({ vizId }) => {
    const containerRef = useRef<HTMLDivElement>(null);
    const networkRef = useRef<Network | null>(null);
    
    useEffect(() => {
        if (!containerRef.current) return;
        
        // Initialize VisJS network
        const options = {
            layout: {
                hierarchical: {
                    direction: 'UD',  // Top-down
                    sortMethod: 'directed'
                }
            },
            nodes: {
                shape: 'box',
                font: { color: '#ffffff' }
            },
            edges: {
                arrows: 'to'
            }
        };
        
        networkRef.current = new Network(containerRef.current, { nodes: [], edges: [] }, options);
        
        // Subscribe to updates
        window.CluicheEditor.subscribeVisualization(vizId, (data: any) => {
            updateTree(data);
        });
        
        return () => {
            networkRef.current?.destroy();
        };
    }, [vizId]);
    
    const updateTree = (data: any) => {
        if (!networkRef.current || !data.processingUnits) return;
        
        // Decision 40: Read-only (no inline editing)
        const nodes = data.processingUnits.map((pu: any) => ({
            id: pu.id,
            label: pu.name,
            color: getStateColor(pu.state)  // Running=green, Stopped=gray, Error=red
        }));
        
        const edges = data.processingUnits
            .filter((pu: any) => pu.parentId)
            .map((pu: any) => ({
                from: pu.parentId,
                to: pu.id
            }));
        
        networkRef.current.setData({ nodes, edges });
    };
    
    const getStateColor = (state: string): string => {
        switch (state) {
            case 'Running': return '#44ff44';
            case 'Stopped': return '#888888';
            case 'Error': return '#ff4444';
            default: return '#ffffff';
        }
    };
    
    return <div ref={containerRef} style={{ width: '100%', height: '400px' }} />;
};
```

**FPS Chart (recharts):**
```typescript
// Decision 42: Core monitoring - FPS over time
// Decision 38: recharts for time-series
export const FPSChart: React.FC<{ vizId: string }> = ({ vizId }) => {
    const [fpsData, setFpsData] = useState<{ time: number; fps: number }[]>([]);
    const maxDataPoints = 100;  // Last 100 samples
    
    useEffect(() => {
        window.CluicheEditor.subscribeVisualization(vizId, (data: any) => {
            if (data.fps !== undefined) {
                setFpsData(prev => {
                    const newData = [...prev, { time: Date.now(), fps: data.fps }];
                    // Keep last 100 points
                    if (newData.length > maxDataPoints) {
                        return newData.slice(newData.length - maxDataPoints);
                    }
                    return newData;
                });
            }
        });
    }, [vizId]);
    
    return (
        <LineChart width={600} height={300} data={fpsData}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis dataKey="time" hide />
            <YAxis domain={[0, 120]} />
            <Tooltip />
            <Legend />
            <Line type="monotone" dataKey="fps" stroke="#44ff44" dot={false} />
        </LineChart>
    );
};
```

**Memory Chart (recharts):**
```typescript
// Decision 42: Core monitoring - Memory usage over time
export const MemoryChart: React.FC<{ vizId: string }> = ({ vizId }) => {
    const [memoryData, setMemoryData] = useState<{ time: number; usedMB: number }[]>([]);
    const maxDataPoints = 100;
    
    useEffect(() => {
        window.CluicheEditor.subscribeVisualization(vizId, (data: any) => {
            if (data.memoryUsedBytes !== undefined) {
                const usedMB = data.memoryUsedBytes / (1024 * 1024);
                setMemoryData(prev => {
                    const newData = [...prev, { time: Date.now(), usedMB }];
                    if (newData.length > maxDataPoints) {
                        return newData.slice(newData.length - maxDataPoints);
                    }
                    return newData;
                });
            }
        });
    }, [vizId]);
    
    return (
        <LineChart width={600} height={300} data={memoryData}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis dataKey="time" hide />
            <YAxis />
            <Tooltip />
            <Legend />
            <Line type="monotone" dataKey="usedMB" stroke="#ffcc00" dot={false} />
        </LineChart>
    );
};
```

**Frame Time Chart (recharts):**
```typescript
// Decision 42: Core monitoring - Frame time over time
export const FrameTimeChart: React.FC<{ vizId: string }> = ({ vizId }) => {
    const [frameTimeData, setFrameTimeData] = useState<{ time: number; frameMs: number }[]>([]);
    const maxDataPoints = 100;
    
    useEffect(() => {
        window.CluicheEditor.subscribeVisualization(vizId, (data: any) => {
            if (data.frameTimeMs !== undefined) {
                setFrameTimeData(prev => {
                    const newData = [...prev, { time: Date.now(), frameMs: data.frameTimeMs }];
                    if (newData.length > maxDataPoints) {
                        return newData.slice(newData.length - maxDataPoints);
                    }
                    return newData;
                });
            }
        });
    }, [vizId]);
    
    return (
        <LineChart width={600} height={300} data={frameTimeData}>
            <CartesianGrid strokeDasharray="3 3" />
            <XAxis dataKey="time" hide />
            <YAxis domain={[0, 33]} />  {/* 33ms = 30 FPS */}
            <Tooltip />
            <Legend />
            <Line type="monotone" dataKey="frameMs" stroke="#4499ff" dot={false} />
            {/* 16ms reference line (60 FPS) */}
            <Line type="monotone" dataKey={() => 16} stroke="#888888" strokeDasharray="5 5" dot={false} />
        </LineChart>
    );
};
```

### C++ Visualization Framework

**IVisualization Interface:**
```cpp
// Dia/DiaEditor/Visualization/IVisualization.h
namespace Dia::Editor {
    enum class VisualizationType {
        kHierarchy,    // VisJS network graphs
        kTimeSeries,   // recharts line/bar charts
        kCustom        // Custom rendering
    };
    
    class IVisualization {
    public:
        virtual ~IVisualization() = default;
        
        // Metadata
        virtual const char* GetId() const = 0;
        virtual const char* GetName() const = 0;
        virtual VisualizationType GetType() const = 0;
        
        // Data subscription (Decision 42: pluggable framework)
        virtual const char* GetDataSubscription() const = 0;  // e.g., "processingUnits", "performance"
        
        // Data transformation (game data → UI-friendly format)
        virtual Json::Value TransformData(const Json::Value& gameData) const = 0;
    };
    
    class IVisualizationFactory {
    public:
        virtual ~IVisualizationFactory() = default;
        virtual IVisualization* Create() = 0;
    };
}
```

**VisualizationRegistry:**
```cpp
// Dia/DiaEditor/Visualization/VisualizationRegistry.h
namespace Dia::Editor {
    class VisualizationRegistry {
    public:
        static VisualizationRegistry& Instance();
        
        void RegisterVisualization(const StringCRC& typeId, IVisualizationFactory* factory);
        IVisualization* CreateVisualization(const StringCRC& typeId);
        void UnregisterVisualization(const StringCRC& typeId);
        
        const DynamicArrayC<StringCRC, 32>& GetRegisteredVisualizations() const;
        bool HasVisualization(const StringCRC& typeId) const;
        
        // Get all visualizations as JSON (for UI)
        Json::Value GetAllVisualizationsMetadata() const;
    };
}
```

**Registration Macro:**
```cpp
// Mirrors EditorPlugin registration pattern
#define REGISTER_VISUALIZATION(ClassName, TypeName) \
    namespace { \
        struct ClassName##Factory : public IVisualizationFactory { \
            IVisualization* Create() override { return new ClassName(); } \
        }; \
        static ClassName##Factory g_##ClassName##Factory; \
        struct ClassName##Registrar { \
            ClassName##Registrar() { \
                VisualizationRegistry::Instance().RegisterVisualization( \
                    Dia::Core::StringCRC(TypeName), &g_##ClassName##Factory); \
            } \
        }; \
        static ClassName##Registrar g_##ClassName##Registrar; \
    }

// Usage example:
// REGISTER_VISUALIZATION(ProcessingUnitTreeVisualization, "ProcessingUnitTree")
```

**Core Visualization Implementations:**
```cpp
// Dia/DiaEditor/Visualization/ProcessingUnitTreeVisualization.h
namespace Dia::Editor {
    class ProcessingUnitTreeVisualization : public IVisualization {
    public:
        const char* GetId() const override { return "ProcessingUnitTree"; }
        const char* GetName() const override { return "ProcessingUnit Tree"; }
        VisualizationType GetType() const override { return VisualizationType::kHierarchy; }
        
        const char* GetDataSubscription() const override { return "processingUnits"; }
        
        Json::Value TransformData(const Json::Value& gameData) const override {
            // Transform game data to VisJS format
            Json::Value result;
            result["processingUnits"] = Json::arrayValue;
            
            // Assume gameData contains ProcessingUnit hierarchy
            for (const auto& pu : gameData["processingUnits"]) {
                Json::Value node;
                node["id"] = pu["id"];
                node["name"] = pu["name"];
                node["state"] = pu["state"];  // "Running", "Stopped", "Error"
                node["parentId"] = pu.get("parentId", "");
                result["processingUnits"].append(node);
            }
            
            return result;
        }
    };
    
    REGISTER_VISUALIZATION(ProcessingUnitTreeVisualization, "ProcessingUnitTree")
}
```

```cpp
// Dia/DiaEditor/Visualization/FPSChartVisualization.h
namespace Dia::Editor {
    class FPSChartVisualization : public IVisualization {
    public:
        const char* GetId() const override { return "FPSChart"; }
        const char* GetName() const override { return "FPS Chart"; }
        VisualizationType GetType() const override { return VisualizationType::kTimeSeries; }
        
        const char* GetDataSubscription() const override { return "performance"; }
        
        Json::Value TransformData(const Json::Value& gameData) const override {
            Json::Value result;
            result["fps"] = gameData.get("fps", 0.0);
            return result;
        }
    };
    
    REGISTER_VISUALIZATION(FPSChartVisualization, "FPSChart")
}
```

```cpp
// Dia/DiaEditor/Visualization/MemoryChartVisualization.h
namespace Dia::Editor {
    class MemoryChartVisualization : public IVisualization {
    public:
        const char* GetId() const override { return "MemoryChart"; }
        const char* GetName() const override { return "Memory Chart"; }
        VisualizationType GetType() const override { return VisualizationType::kTimeSeries; }
        
        const char* GetDataSubscription() const override { return "memory"; }
        
        Json::Value TransformData(const Json::Value& gameData) const override {
            Json::Value result;
            result["memoryUsedBytes"] = gameData.get("usedBytes", 0);
            return result;
        }
    };
    
    REGISTER_VISUALIZATION(MemoryChartVisualization, "MemoryChart")
}
```

```cpp
// Dia/DiaEditor/Visualization/FrameTimeChartVisualization.h
namespace Dia::Editor {
    class FrameTimeChartVisualization : public IVisualization {
    public:
        const char* GetId() const override { return "FrameTimeChart"; }
        const char* GetName() const override { return "Frame Time Chart"; }
        VisualizationType GetType() const override { return VisualizationType::kTimeSeries; }
        
        const char* GetDataSubscription() const override { return "performance"; }
        
        Json::Value TransformData(const Json::Value& gameData) const override {
            Json::Value result;
            result["frameTimeMs"] = gameData.get("frameTimeMs", 0.0);
            return result;
        }
    };
    
    REGISTER_VISUALIZATION(FrameTimeChartVisualization, "FrameTimeChart")
}
```

### Update Throttling

**C++ Side (Decision 39: 10 FPS throttle):**
```cpp
// Dia/DiaEditor/Visualization/VisualizationManager.h
namespace Dia::Editor {
    class VisualizationManager : public Dia::Application::Module {
    public:
        VisualizationManager(ProcessingUnit* pu);
        
        // Subscribe to game data updates
        void SubscribeToGameData();
        
        // Receive updates from GameConnectionManager
        void OnGameDataReceived(const Json::Value& data);
        
        // Send updates to UI (with throttling)
        void SendToUI(const StringCRC& visualizationId, const Json::Value& data);
        
    protected:
        void DoUpdate(float deltaTime) override;
        
    private:
        // Decision 39: 10 FPS update throttle (100ms between updates)
        static constexpr float kUpdateThrottleSeconds = 0.1f;
        float mTimeSinceLastUpdate;
        
        GameConnectionManager* mGameConnection;
        
        struct ActiveVisualization {
            StringCRC id;
            IVisualization* visualization;
        };
        DynamicArrayC<ActiveVisualization, 16> mActiveVisualizations;
        Json::Value mPendingData;
    };
}
```

**Implementation:**
```cpp
void VisualizationManager::DoUpdate(float deltaTime) {
    mTimeSinceLastUpdate += deltaTime;
    
    // Decision 39: Throttle to 10 FPS (100ms)
    if (mTimeSinceLastUpdate >= kUpdateThrottleSeconds) {
        mTimeSinceLastUpdate = 0.0f;
        
        // Process pending data and send to UI
        if (!mPendingData.empty()) {
            for (int i = 0; i < mActiveVisualizations.Size(); ++i) {
                Json::Value transformedData = mActiveVisualizations[i].visualization->TransformData(mPendingData);
                SendToUI(mActiveVisualizations[i].id, transformedData);
            }
            
            mPendingData.clear();
        }
    }
}
```

## Implementation Files

- `Dia/DiaEditor/Visualization/IVisualization.h` - Visualization interface
- `Dia/DiaEditor/Visualization/VisualizationRegistry.h/cpp` - Registration system
- `Dia/DiaEditor/Visualization/VisualizationManager.h/cpp` - Update throttling and data management
- `Dia/DiaEditor/Visualization/ProcessingUnitTreeVisualization.h` - Core: PU tree
- `Dia/DiaEditor/Visualization/FPSChartVisualization.h` - Core: FPS chart
- `Dia/DiaEditor/Visualization/MemoryChartVisualization.h` - Core: Memory chart
- `Dia/DiaEditor/Visualization/FrameTimeChartVisualization.h` - Core: Frame time chart
- `Cluiche/CluicheEditor/UI/src/components/LiveDataPanel.tsx` - React UI component
- `Cluiche/CluicheEditor/UI/src/components/ProcessingUnitTree.tsx` - VisJS tree visualization
- `Cluiche/CluicheEditor/UI/src/components/FPSChart.tsx` - recharts FPS visualization
- `Cluiche/CluicheEditor/UI/src/components/MemoryChart.tsx` - recharts memory visualization
- `Cluiche/CluicheEditor/UI/src/components/FrameTimeChart.tsx` - recharts frame time visualization
- `Cluiche/CluicheEditor/UI/package.json` - Add `vis-network`, `recharts` dependencies

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — visualization IDs and data subscriptions use StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — VisualizationManager extends Module with DoUpdate |
| Platform | PD-004 | No STL in public APIs | **Compliant** — IVisualization uses `const char*`, Json::Value; VisualizationManager uses DynamicArrayC not std::map |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — built within DiaEditor .vcxproj |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004; ActiveVisualization uses StringCRC not std::string |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — all types in `Dia::Editor::` |
| Dia | AD-004 | PU/Phase/Module for apps | **Compliant** — VisualizationManager participates in PU update loop |
| DiaEditor | SED-001 | Plugin interface minimal and stable | **Compliant** — IVisualization has 5 methods |
| DiaEditor | SED-002 | Plugins register via macro | **Compliant** — REGISTER_VISUALIZATION macro mirrors REGISTER_EDITOR_PLUGIN |
| DiaEditor | SED-005 | CEF replaces Awesomium | **Compliant** — React visualizations render in CEF |
| DiaEditor | SED-010 | Use DiaDebugProtocol for wire types | **Compliant** — game data arrives via GameConnectionManager using protocol types |

**All binding decisions: COMPLIANT**

## Open Questions

**Resolved:**
- **Decision 38:** Hybrid rendering: VisJS for hierarchies (ProcessingUnit tree), recharts for time-series charts (FPS, Memory, Frame Time)
- **Decision 39:** 10 FPS update throttle (redraw every 100ms) to avoid editor performance impact
- **Decision 40:** Read-only visualizations (no inline editing of game state)
- **Decision 41:** Filter by ProcessingUnit dropdown to focus on specific subsystems
- **Decision 42:** Pluggable framework with IVisualization interface + core monitoring base set (Tree, FPS, Memory, Frame Time)

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Rendering | Which libraries for visualization? | VisJS (hierarchy) + recharts (time-series) | ✅ Hybrid (Decision 38) |
| 2 | Performance | Update throttle rate? | 10 FPS (100ms) | ✅ 10 FPS (Decision 39) |
| 3 | Interaction | Inline editing support? | No, read-only | ✅ Read-only (Decision 40) |
| 4 | Filtering | Filter by ProcessingUnit? | Yes, dropdown filter | ✅ Dropdown (Decision 41) |
| 5 | Extensibility | Pluggable visualization system? | Yes, IVisualization interface | ✅ Pluggable (Decision 42) |
| 6 | Core Set | Which visualizations by default? | Tree, FPS, Memory, Frame Time | ✅ Core set (Decision 42) |
| 7 | Data Source | WebSocket subscription? | Yes, via GameConnectionManager | ✅ Implemented |
| 8 | Color Coding | State colors? | Running=green, Stopped=gray, Error=red | ✅ Implemented |

## Status

`Approved` - Ready for implementation
