# Asset Style Guide

## Directory Layout

All game assets live under the application directory:

```
Cluiche/Assets/<AppName>/
├── cluicheeditor.diaobservation    (metadata: editor state)
├── Stages/
│   ├── MainMenu.diastage
│   ├── Level1.diastage
│   └── Level2.diastage
├── Sprites/
│   ├── player.png
│   ├── player_idle_left.png
│   ├── enemy_goblin.png
│   └── ...
├── Audio/
│   ├── bgm_main_menu.ogg
│   ├── sfx_jump.ogg
│   └── ...
├── Animations/
│   ├── Player_Idle.animation
│   ├── Player_Run.animation
│   └── ...
├── GameData/
│   └── EntityTemplates/
│       ├── Player.yaml
│       ├── Goblin.yaml
│       └── ...
└── my_game.diagame                 (root manifest)
```

- **AppName** examples: `CluicheTest`, `CluicheEditor`, `MyGame`
- Top-level manifest (`.diagame`) lives at root

## Naming Conventions

### Directory Names: PascalCase
- `Sprites/`, `Audio/`, `Animations/`, `Stages/`, `GameData/`
- Example: `EntityTemplates/`, `Shaders/`, `Particles/`
- **Reason:** Visual distinction in file browser; easier to scan

### Asset Files: snake_case
- Images: `player.png`, `player_idle_left.png`, `enemy_goblin.png`
- Audio: `bgm_main_menu.ogg`, `sfx_jump.ogg`, `voice_dialogue_001.ogg`
- Animations: `player_idle.animation`, `player_run.animation`
- Data: `player_template.yaml`, `enemy_config.yaml`
- **Reason:** Readable in manifests, compatible with scripting

### Manifest Files: snake_case with extension
- Games: `my_game.diagame`
- Stages: `main_menu.diastage`
- Entities: `player.yaml`, `enemy_goblin.yaml`
- **Reason:** Matches asset naming; clear file type from extension

### Entity Template Names: PascalCase
In manifests and in-editor references:
- `Player`, `Goblin`, `MainMenuCamera`, `TitleBackground`
- **Reason:** Distinguishes logical entity types from filename convention

## Entity References in Manifests

When referencing entities, use template names:

```yaml
# .diastage file
entities:
  - entity_id: "player_1"
    template: "Player"          # PascalCase template name
    asset: "player.png"         # snake_case asset filename
    animation: "player_idle"    # snake_case animation filename
```

Entity IDs are lowercase with underscores (e.g., `player_1`, `enemy_goblin_03`).

## Asset Metadata Rules

### Image/Sprite Assets
Each sprite should have metadata in its manifest entry:

```yaml
- type: "render.sprite"
  asset: "player_idle.png"       # filename (snake_case)
  material: "opaque"             # opaque | transparent | additive
  z_order: 10                    # depth for layering (0-100)
  scale: [1.0, 1.0]              # pixel-perfect scale
```

### Animation Assets
Each animation entry in a stage references animation files:

```yaml
- type: "animation.animator"
  animation: "player_idle"       # animation file (no .animation extension)
  speed: 1.0                     # playback speed multiplier
  loop: true                     # loop mode
```

Animation files live in `Animations/` as `<name>.animation` files.

### Audio Assets
Audio source components reference sound files:

```yaml
- type: "audio.source"
  clip: "sfx_jump"               # audio file (no .ogg extension)
  volume: 1.0                    # 0-1 scale
  spatial: true                  # 3D positioning if true
```

Audio files live in `Audio/` as `<name>.ogg` files.

## Adding a New Stage to a Game

### 1. Create Stage Manifest
Create `Cluiche/Assets/MyGame/Stages/my_level.diastage`:

```yaml
version: "2.0"
stage_id: "my_level"
entities:
  - entity_id: "player_1"
    template: "Player"
    position: [0, 0]
    components:
      - type: "physics.rigid_body"
        mass: 1.0
      - type: "render.sprite"
        asset: "player.png"
  - entity_id: "enemy_1"
    template: "Goblin"
    position: [10, 0]
    components:
      - type: "render.sprite"
        asset: "enemy_goblin.png"
```

### 2. Register in Game Manifest
Edit `Cluiche/Assets/MyGame/my_game.diagame`:

```yaml
version: "2.0"
app_id: "my_game"
start_stage: "main_menu"
stages:
  - id: "my_level"
    file: "Stages/my_level.diastage"
  - id: "main_menu"
    file: "Stages/MainMenu.diastage"
```

### 3. Validate Manifests
```bash
dia validate manifest --path Cluiche/Assets/MyGame/my_game.diagame
```

Checks for:
- Missing required fields
- Invalid stage references
- Malformed YAML
- Schema compliance

### 4. Load in Editor
Open the game manifest in CluicheEditor:
1. **File → Open** → select `my_game.diagame`
2. **Stage Selector** dropdown shows all registered stages
3. Click **Load Stage** to view entities
4. Edit in Scene View panel; auto-saves to manifest

## Module Documentation File Naming

For Dia engine modules, documentation files follow the pattern:

```
Dia/<LayerName>/<ModuleName>/dia.<parent>.<module>.architecture.module.md
```

Examples:
- `Dia/DiaCore/ProcessingUnit/dia.core.processingunit.architecture.module.md`
- `Dia/DiaGraphics/Render2D/dia.graphics.render2d.architecture.module.md`
- `Dia/DiaPhysics/Physics2D/dia.physics.physics2d.architecture.module.md`

These files contain YAML frontmatter describing:
- Module ID and namespaces
- Public API (exported headers)
- Dependencies
- Responsibilities and non-responsibilities

## Validation Checklist

Before committing a new asset or stage:

- [ ] Directory and file names follow conventions (PascalCase dirs, snake_case files)
- [ ] All referenced assets exist (sprites, audio, animations)
- [ ] Entity templates are PascalCase in manifests
- [ ] Manifest passes `dia validate manifest`
- [ ] No hardcoded absolute paths (use relative paths from app root)
- [ ] Component types are registered in `ComponentFactoryRegistry`
- [ ] Metadata (z_order, material, etc.) matches component schema

## Performance Guidelines

- Keep stage entity counts under 500 for smooth 60 FPS on target hardware
- Batch similar render types (same material, same z_order)
- Use sprite sheets instead of individual images when possible
- Compress audio to OGG format; target 64-128 kbps bitrate
- Preload large assets during loading screens, not during gameplay
