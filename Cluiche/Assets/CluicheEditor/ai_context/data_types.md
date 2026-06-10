# Data Types and Asset Reference

## Entity Structure

### DiaEntity

A **DiaEntity** is a runtime game object that holds a collection of **components**:

```cpp
DiaEntity {
  StringCRC entity_id;           // Unique identifier (StringCRC)
  DynamicArrayC<IComponent*> components;  // Attached components
  Transform position;            // World position and rotation
  bool active;                   // Runtime flag
}
```

- Entities are created via `EntityFactory` with a template reference
- Components are added/removed dynamically
- Queried by position, ID, or component type
- Serialized to `.diastage` manifest under `entities[]` array

### IComponent Interface

All components inherit from **IComponent**:

```cpp
class IComponent {
  virtual StringCRC GetTypeId() const;          // Component type (e.g., "physics.rigid_body")
  virtual void OnAttach(DiaEntity* owner);      // Called when added to entity
  virtual void OnDetach();                       // Called when removed
  virtual void OnUpdate(float dt);               // Per-frame update
  virtual Json Serialize() const;                // Save to manifest
  virtual void Deserialize(const Json& data);    // Load from manifest
};
```

Components are **composable** — an entity can have multiple components (e.g., Render + Physics + Animation).

## Built-in Component Types

### Transform Component (`transform`)
- Stores position, rotation, scale
- Every entity has one (implicit)
- Updated by physics or animation modules
- Queried by spatial queries (e.g., "get entities in bounding box")

### Physics Component (`physics.rigid_body`)
- Rigid body with mass, friction, damping
- Collision detection and response
- Kinematic or dynamic (velocity-driven)
- Output: velocity, angular velocity updated each frame

### Render Component (`render.sprite` / `render.mesh`)
- Associates mesh or sprite asset with entity
- Material and shader parameters
- Z-order / layer for rendering
- Queried by render system each frame

### Animation Component (`animation.animator`)
- Playback state for skeletal or sprite animations
- Current frame, speed, loop mode
- Driven by `Animation2D` or skeleton system
- Output: updates Transform position / rotation per frame

### Audio Component (`audio.source`)
- 3D spatial audio source
- Plays sound clips on cue
- Volume, pitch, attenuation curve
- Attached to entity Transform for spatialization

## Asset Types

### `.diagame` — Game Manifest
Top-level manifest for a game project. YAML format:

```yaml
version: "2.0"
app_id: "my_game"
start_stage: "main_menu"
stages:
  - id: "main_menu"
    file: "Stages/MainMenu.diastage"
  - id: "level_1"
    file: "Stages/Level1.diastage"
```

- Defines startup stage, stage list, global game parameters
- Validated with `dia validate manifest --path <file>.diagame`

### `.diastage` — Stage Manifest
Defines a single playable level or scene. YAML format:

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
      - type: "animation.animator"
        animation: "player_idle"
```

- Lists all entities and their components
- Referenced by `.diagame` manifest
- Loaded at runtime into `ProcessingUnit`

### `.diaapp` — Application Manifest
Similar to `.diagame` but for tools/editors (e.g., CluicheEditor itself).

### `.diamodule` — Module Definition
Metadata for a Dia engine module (used by build system, not at runtime).

## Component Type Registry

The **ComponentFactoryRegistry** maps `StringCRC` type IDs to factory functions:

```cpp
// Registration (typically in module OnLoad):
ComponentFactoryRegistry::Register(
  StringCRC("physics.rigid_body"),
  []() -> IComponent* { return new RigidBodyComponent(); }
);

// Instantiation at entity creation:
auto type_id = StringCRC("physics.rigid_body");
auto factory = ComponentFactoryRegistry::GetFactory(type_id);
auto component = factory->CreateComponent();
entity->AttachComponent(component);
```

- Factories are registered per-module
- Type IDs are string-based `StringCRC` for readability + performance
- Registry loaded during engine startup

## Data Serialization (JSON/YAML)

All manifests and component data are serialized to JSON/YAML:

- **Parser:** jsoncpp library (wrapped in `Dia::Core::Json` namespace)
- **Format:** YAML for manifests (human-readable), JSON for internal data
- **Component data:** Each component serializes its state to a `Json` object nested in the entity's manifest
- **Validation:** `dia validate manifest` checks schema compliance

## StringCRC Usage

`StringCRC` is a **compile-time hashed string identifier**:

```cpp
constexpr StringCRC kTypeId = StringCRC("physics.rigid_body");

// Comparison (O(1) integer comparison):
if (component.GetTypeId() == kTypeId) { ... }

// Used for:
// - Component type IDs
// - Event names
// - Module IDs
// - Asset reference keys
```

**Benefits:** No string allocations, O(1) lookup, type-safe at compile time.

## Example: Complete Entity in Manifest

```yaml
entity_id: "enemy_1"
template: "Goblin"
position: [10.5, 5.0]
rotation: 0
scale: [1.0, 1.0]
components:
  - type: "physics.rigid_body"
    mass: 0.5
    friction: 0.3
    shape: "circle"
    radius: 0.5
  - type: "render.sprite"
    asset: "Goblin.png"
    material: "opaque"
    z_order: 10
  - type: "animation.animator"
    animation: "goblin_idle"
    speed: 1.0
  - type: "audio.source"
    clip: "goblin_growl"
    volume: 0.8
```

Loaded at runtime:
1. Entity created with ID `enemy_1`
2. Components instantiated via `ComponentFactoryRegistry`
3. Each component deserialized from its YAML block
4. Added to entity; `OnAttach()` called
5. Ready for simulation
