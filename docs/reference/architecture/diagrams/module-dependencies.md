# Module Dependencies Diagram

This diagram shows the module dependencies within each processing unit and the cross-thread communication patterns:

- **Main Thread Modules** (blue) - UI, level management, input collection
- **Render Thread Modules** (red) - Frame rendering and canvas management
- **Sim Thread Modules** (green) - Simulation logic, time, and input processing

```mermaid
graph TD
    subgraph MainThread["Main Thread (MainProcessingUnit)"]
        MainKernel["MainKernelModule<br/>• TimeServer @ 30Hz<br/>• InputSourceManager<br/>• SFML Window<br/>• Canvas"]
        LevelFactory["LevelFactoryModule<br/>• Level Registry<br/>• Level Creation"]
        MainUI["MainUIModule<br/>• UI System<br/>• Observer Subject<br/>• UI Pages"]
    end

    subgraph RenderThread["Render Thread (RenderProcessingUnit)"]
        RenderKernel["RenderKernel<br/>• Frame Management"]
        RenderCanvas["RenderCanvas<br/>• SFML Rendering<br/>• Display @ 60 FPS"]
    end

    subgraph SimThread["Sim Thread (SimProcessingUnit)"]
        SimTimeServer["SimTimeServerModule<br/>• Simulation Clock<br/>• Independent Time"]
        SimInputFrame["SimInputFrameStreamModule<br/>• Reads InputToSimFrameStream<br/>• Event Consumer"]
        SimUIProxy["SimUIProxyModule<br/>• Observer (MainUI)<br/>• Cross-thread UI bridge<br/>• Mutex-protected"]
    end

    %% Main Thread Dependencies
    LevelFactory --> MainKernel
    MainUI --> MainKernel

    %% Render Thread Dependencies
    RenderCanvas --> RenderKernel

    %% Sim Thread Dependencies
    SimUIProxy --> SimTimeServer
    SimInputFrame --> SimTimeServer

    %% Cross-Thread Dependencies (Observer Pattern)
    MainUI -.Observer Pattern.-> SimUIProxy
    MainUI -.Mutex Protected.-> SimUIProxy

    %% Data Flow
    MainKernel -.Input Events.-> SimInputFrame
    SimThread -.Graphics Commands.-> RenderCanvas

    %% Styling
    classDef mainStyle fill:#4A90E2,stroke:#2E5C8A,stroke-width:2px,color:#fff
    classDef renderStyle fill:#E94B3C,stroke:#C62828,stroke-width:2px,color:#fff
    classDef simStyle fill:#6BBE45,stroke:#388E3C,stroke-width:2px,color:#fff

    class MainKernel,LevelFactory,MainUI mainStyle
    class RenderKernel,RenderCanvas renderStyle
    class SimTimeServer,SimInputFrame,SimUIProxy simStyle

    %% Notes
    Note1["MainKernel provides:<br/>- Time (30Hz)<br/>- Input collection<br/>- Window/Canvas"]
    Note2["RenderCanvas reads:<br/>- SimToRenderFrameStream<br/>- Graphics commands"]
    Note3["SimInputFrame reads:<br/>- InputToSimFrameStream<br/>- Input events"]

    Note1 -.-> MainKernel
    Note2 -.-> RenderCanvas
    Note3 -.-> SimInputFrame
```

[← Back to Architecture Overview](../architecture.md)
