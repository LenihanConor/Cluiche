# DiaArchitecture — Target Solution Folder Layout

**Parent:** [diaarchitecture.md](diaarchitecture.md)

This is the target assignment of every Dia `.vcxproj` to a numbered Visual Studio solution folder. Derived from the `layer:` field on each module doc. Maintained by `dia check sln-sync`.

Projects with no module doc (DiaImGui, DiaPicking, DiaGeometry2DPicking, DiaProtobuf, DiaSDL, DiaAsset, DiaGame) remain in `Library` until their module docs are created.

---

## Foundation

### 1.0-Core
`DiaCore` `DiaFileIO` `DiaJson` `DiaMailbox` `DiaSerializer` `DiaStateMachine` `DiaThreading`

### 1.1-Maths
`DiaMaths` `DiaGeometry2D`

### 1.1-Services
`DiaDebugDraw` `DiaDebugProtocol` `DiaDebugServer` `DiaObservation` `DiaPython` `DiaStreams` `DiaWebSocket`

### 1.2-Platform
`DiaInput` `DiaWindow`

### 1.2-Application
`DiaApplicationFlow` `DiaAutomation`

---

## Assets

### 2.0-Assets
`DiaAssetCatalogue` `DiaAssetRuntime` `diaentitytemplate`

### 2.1-Assets-Tools
`DiaAssetCatalogueEditor` `DiaAssetRuntimeInspector` `DiaEntityVisualDebugger`

---

## Domains

### 3.0-Visual
`DiaBgfx` `DiaCamera2D` `DiaGraphics` `DiaLighting2D` `DiaScene2D` `DiaUI` `DiaUICEF` `DiaUIUltralight`

### 3.1-Visual-Tools
`DiaGeometry2DVisualDebugger` `DiaVisualDebugger`

### 3.0-Physics
`DiaRigidBody2D` `DiaSoftBody2D`

### 3.1-Physics-Tools
`DiaRigidBody2DVisualDebugger` `DiaSoftBody2DVisualDebugger`

### 3.0-Animation
`DiaAnimation2D` `DiaIK2D` `DiaRig2D`

### 3.1-Animation-Tools
`DiaAnimation2DVisualDebugger` `DiaIK2DVisualDebugger` `DiaRig2DVisualDebugger`

---

## Unassigned (no module doc yet)

These stay in `Library` until a module doc with `layer:` is created. `dia check sln-sync` will move them automatically once the doc exists.

| Project | Expected layer |
|---------|---------------|
| DiaAsset | assets/core → 2.0-Assets |
| DiaGame | foundation/application → 1.2-Application |
| DiaGeometry2DPicking | foundation/maths → 1.1-Maths |
| DiaGeometry3D | foundation/maths → 1.1-Maths |
| DiaImGui | foundation/services → 1.1-Services |
| DiaPicking | foundation/core → 1.0-Core |
| DiaProtobuf | foundation/core → 1.0-Core |
| DiaSDL | foundation/platform → 1.2-Platform |

---

## Non-Dia projects (unchanged)

These are not Dia library modules and keep their existing folders:

| Project | Folder |
|---------|--------|
| CluicheTest | _Executables |
| GoogleTests | _Executables |
| CluicheGameBaseline | _Executables |
| CluicheEditor | _Executables |
| DiaApplicationEditor | Editors |
| DiaEntityTemplateEditor | Editors |
| DiaPipelineEditor | Editors |
| DiaSceneEditor | Editors |
| DiaVisualDebuggerConsole | VisualDebuggers |
| DiaScene2DVisualDebugger | VisualDebuggers |
| DiaAssetRuntimeVisualDebugger | VisualDebuggers |
| DiaCLI | Library (solution items folder) |
