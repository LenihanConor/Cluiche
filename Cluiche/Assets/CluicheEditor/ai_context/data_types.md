# Data Types and Asset Reference

## Entity Structure

### DiaEntity

A **DiaEntity** is a runtime game object that holds a collection of **components**:

```cpp
DiaEntity {
  StringCRC entity_id;           // Unique identifier (StringCRC)
  DynamicArrayC<IComponent*> components;
  Transform position;            // World position and rotation
  bool active;                   // Runtime flag
}
```

- Created via `EntityFactory` with a template reference
- Components added/removed dynamically
- Serialised to `.diastage` under `entities[]`

### IComponent Interface

```cpp
class IComponent {
  virtual StringCRC GetTypeId() const;       // e.g. StringCRC("physics.rigid_body")
  virtual void OnAttach(DiaEntity* owner);
  virtual void OnDetach();
  virtual void OnUpdate(float dt);
  virtual Json Serialize() const;
  virtual void Deserialize(const Json& data);
};
```

## Built-in Component Types

| Type ID | Description |
|---------|-------------|
| `transform` | Position, rotation, scale — implicit on every entity |
| `physics.rigid_body` | Mass, friction, damping; kinematic or dynamic |
| `render.sprite` | Sprite asset, material, z-order |
| `render.mesh` | 3D mesh asset and material (DiaMesh3D) |
| `animation.animator` | Sprite/skeletal animation playback state |
| `audio.source` | 3D spatial audio source |

## Asset File Types

### `.diagame` — Game Manifest
```yaml
version: "2.0"
app_id: "my_game"
start_stage: "main_menu"
stages:
  - id: "main_menu"
    file: "Stages/MainMenu.diastage"
```

### `.diastage` — Stage Manifest
```yaml
version: "2.0"
stage_id: "level_1"
entities:
  - entity_id: "player_1"
    template: "Player"
    position: [0, 0]
    components:
      - type: "physics.rigid_body"
        mass: 1.0
      - type: "render.sprite"
        asset: "Player.png"
```

### `.diaapp` — Application Manifest
Same shape as `.diagame`; used for tools and editors (e.g. CluicheEditor itself).

## Component Type Registry

`ComponentFactoryRegistry` maps `StringCRC` type IDs to factory functions:

```cpp
ComponentFactoryRegistry::Register(
  StringCRC("physics.rigid_body"),
  []() -> IComponent* { return new RigidBodyComponent(); }
);
```

## Serialisation

- **Manifests:** YAML (human-readable)
- **Internal data:** JSON via jsoncpp (`Dia::Core::Json`)
- **Validation:** `dia validate manifest --path <file>`

## StringCRC

Compile-time CRC hash — O(1) string comparison, no allocations:

```cpp
constexpr StringCRC kTypeId = StringCRC("physics.rigid_body");
if (component.GetTypeId() == kTypeId) { ... }
```

Used for component type IDs, module IDs, event names, and asset keys.

## Engine Layers

| Layer | Namespace | Purpose |
|-------|-----------|---------|
| DiaCore | `Dia::Core::` | Containers, memory, CRC, reflection, PU/Module/Phase |
| DiaMaths | `Dia::Maths::` | Vectors, matrices, quaternions |
| DiaGraphics | `Dia::Graphics::` | Rendering, camera, geometry |
| DiaPhysics | `Dia::Physics::` | Rigid body, collision |
| DiaSDL | — | Window and input (SDL3; replacing DiaSFML) |
| DiaObservation | — | Logging, tracing, metrics, health |
| DiaEditor | — | Plugin framework, WebUIBridge, EditorActionRegistry |
