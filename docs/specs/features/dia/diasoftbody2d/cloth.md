# Feature Spec: Cloth

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **cloth** |

**Status:** `Approved`

---

## Problem Statement

2D cloth requires a particle grid with three layers of constraints: structural (shape integrity), shear (resistance to skewing), and bend (resistance to folding). It also needs the same tearing and pinning capabilities as a rope, extended to arbitrary grid particles. The grid indexing must be stable and accessible by (x, y) coordinates so game code can read particle positions for rendering and pin/unpin individual particles at runtime.

---

## Solution Overview

`Cloth` is a 2D particle grid constructed from `ClothDef`. Particles are laid out in a `resX × resY` grid starting at `origin`, with horizontal spacing `width / (resX - 1)` and vertical spacing `height / (resY - 1)`. The particle at grid position (x, y) maps to flat index `y * resX + x`.

Three layers of distance constraints are generated at construction:

- **Structural** — horizontal (x, y)↔(x+1, y) and vertical (x, y)↔(x, y+1) pairs; stiffness = `structuralStiffness`
- **Shear** — diagonal (x, y)↔(x+1, y+1) and (x+1, y)↔(x, y+1) pairs; stiffness = `shearStiffness`
- **Bend** — skip-one horizontal (x, y)↔(x+2, y) and vertical (x, y)↔(x, y+2) pairs; stiffness = `bendStiffness`

Tearing is shared with the rope mechanism: per-constraint `maxStretch` threshold; once torn, `IsTorn()` is permanently true. When `pinTopRow = true`, `PinParticle(x, 0)` is called for all `x` at construction. Runtime `PinParticle` and `UnpinParticle` allow game code to change pinned status at any time.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `GetParticleCount()` returns `resX * resY` | Unit test |
| AC2 | `GetParticle(x, y)` returns correct particle; `x` and `y` valid in `[0, resX)` and `[0, resY)` | Unit test |
| AC3 | `GetParticle(x, y)` out-of-range triggers `DIA_ASSERT` in debug | Unit test (debug) |
| AC4 | Particles are laid out in a grid with correct horizontal and vertical spacing at construction | Unit test: measure spacings |
| AC5 | Structural constraints hold the grid shape (particles don't drift apart) under zero gravity over 10 steps | Unit test |
| AC6 | Shear constraints resist diamond collapse: without shear constraints a pure structural grid collapses under lateral displacement; with them it does not | Unit test: compare with shearStiffness=0 vs shearStiffness=1 |
| AC7 | `pinTopRow = true` pins all particles in row y=0 (invMass = 0) at construction; they do not move under gravity | Integration test |
| AC8 | `PinParticle(x, y)` sets `invMass = 0` for that particle at runtime | Unit test |
| AC9 | `UnpinParticle(x, y)` restores the particle's original `invMass` | Unit test |
| AC10 | When `maxStretch = 0.0f`, no constraints are removed regardless of stretch | Unit test |
| AC11 | When `maxStretch > 0.0f` and a constraint is over-stretched, it is removed in `CheckTearing()`; `IsTorn()` becomes true | Unit test |
| AC12 | `IsTorn()` never reverts to false once set | Unit test |
| AC13 | `ClothDef` with `resX * resY > 4096` triggers `DIA_ASSERT` in debug | Unit test (debug) |
| AC14 | `GetId()` returns the `StringCRC` id from `ClothDef` | Unit test |
| AC15 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::SoftBody2D {

struct ClothDef {
    Dia::Core::StringCRC  id;
    Dia::Maths::Vector2D  origin;               // Top-left corner in world space
    float                 width;
    float                 height;
    int                   resX;                 // Particle columns (>= 2)
    int                   resY;                 // Particle rows    (>= 2)
    float                 mass                  = 1.0f;
    float                 structuralStiffness   = 1.0f;  // [0, 1]
    float                 shearStiffness        = 0.8f;  // [0, 1]
    float                 bendStiffness         = 0.3f;  // [0, 1]
    float                 particleRadius        = 0.05f;
    float                 maxStretch            = 0.0f;  // 0 = no tearing
    bool                  pinTopRow             = false; // Pin entire row y=0 at creation
};

class Cloth : public SoftBody {
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"Cloth"};

    explicit Cloth(const ClothDef& def);

    // Particle access — flat index: y * resX + x
    int              GetParticleCount() const;  // resX * resY
    Particle&        GetParticle(int x, int y);
    const Particle&  GetParticle(int x, int y) const;

    // Runtime pinning
    void PinParticle(int x, int y);    // Sets invMass = 0.0f
    void UnpinParticle(int x, int y);  // Restores original invMass

    // State
    bool IsTorn() const;
    const Dia::Core::StringCRC& GetId() const override;

    // Grid dimensions (readable by SoftBodyWorld constraint projection)
    int   GetResX()       const;
    int   GetResY()       const;
    float GetMaxStretch() const;

private:
    Dia::Core::StringCRC                         mId;
    int                                          mResX;
    int                                          mResY;
    Dia::Core::DynamicArrayC<Particle>           mParticles;      // size = resX * resY
    Dia::Core::DynamicArrayC<float>              mOriginalInvMass; // for UnpinParticle restore
    Dia::Core::DynamicArrayC<DistanceConstraint> mConstraints;    // structural + shear + bend
    bool                                         mIsTorn;
    float                                        mMaxStretch;

    int ParticleIndex(int x, int y) const;  // y * mResX + x
};

} // namespace Dia::SoftBody2D
```

---

## Implementation Notes

### Construction

```
DIA_ASSERT(def.resX >= 2 && def.resY >= 2);
DIA_ASSERT(def.resX * def.resY <= 4096);

float dx = (def.resX > 1) ? def.width  / float(def.resX - 1) : 0.0f;
float dy = (def.resY > 1) ? def.height / float(def.resY - 1) : 0.0f;
float perParticleInvMass = (def.mass > 0.0f) ? float(def.resX * def.resY) / def.mass : 0.0f;

for (int y = 0; y < def.resY; ++y) {
    for (int x = 0; x < def.resX; ++x) {
        Particle p;
        p.position = p.prevPosition = def.origin + Vector2D(x * dx, -y * dy);
        p.invMass  = perParticleInvMass;
        p.radius   = def.particleRadius;
        mParticles.PushBack(p);
        mOriginalInvMass.PushBack(perParticleInvMass);
    }
}
```

Grid is laid out with y=0 at the top (origin), increasing y going downward (screen convention).

### Constraint Generation

After all particles are created:

```
// Structural — horizontal
for (int y = 0; y < resY; ++y)
    for (int x = 0; x < resX - 1; ++x)
        AddConstraint(Index(x,y), Index(x+1,y), dx, structuralStiffness);

// Structural — vertical
for (int y = 0; y < resY - 1; ++y)
    for (int x = 0; x < resX; ++x)
        AddConstraint(Index(x,y), Index(x,y+1), dy, structuralStiffness);

// Shear — diagonal down-right
for (int y = 0; y < resY - 1; ++y)
    for (int x = 0; x < resX - 1; ++x)
        AddConstraint(Index(x,y), Index(x+1,y+1), sqrt(dx*dx+dy*dy), shearStiffness);

// Shear — diagonal down-left
for (int y = 0; y < resY - 1; ++y)
    for (int x = 1; x < resX; ++x)
        AddConstraint(Index(x,y), Index(x-1,y+1), sqrt(dx*dx+dy*dy), shearStiffness);

// Bend — skip-one horizontal
for (int y = 0; y < resY; ++y)
    for (int x = 0; x < resX - 2; ++x)
        AddConstraint(Index(x,y), Index(x+2,y), 2*dx, bendStiffness);

// Bend — skip-one vertical
for (int y = 0; y < resY - 2; ++y)
    for (int x = 0; x < resX; ++x)
        AddConstraint(Index(x,y), Index(x,y+2), 2*dy, bendStiffness);
```

### Pinning at Construction

If `def.pinTopRow == true`, call `PinParticle(x, 0)` for all `x` in `[0, resX)` after construction. This sets `invMass = 0.0f` (but `mOriginalInvMass` retains the original value, enabling later `UnpinParticle`).

### Runtime Pinning

```cpp
void Cloth::PinParticle(int x, int y) {
    DIA_ASSERT(x >= 0 && x < mResX && y >= 0 && y < mResY);
    mParticles[ParticleIndex(x, y)].invMass = 0.0f;
}

void Cloth::UnpinParticle(int x, int y) {
    DIA_ASSERT(x >= 0 && x < mResX && y >= 0 && y < mResY);
    int idx = ParticleIndex(x, y);
    mParticles[idx].invMass = mOriginalInvMass[idx];
}
```

### Tearing

Same mechanism as `Rope`: `CheckTearing()` iterates all constraints; any constraint whose current length exceeds `restLength * (1 + maxStretch)` has its `active` flag cleared and `mIsTorn = true`. The XPBD projection loop skips inactive constraints.

### Constraint Projection

Identical XPBD per-constraint update as in the Rope feature. `SoftBodyWorld::ProjectConstraints()` iterates all bodies and all their active constraints `solverIterations` times. The same `DistanceConstraint` internal struct is reused.

### File Layout

```
Dia/DiaSoftBody2D/
├── Cloth.h
└── Cloth.cpp
```

---

## Dependencies

### Required Features (must exist first)
- **particle** — `Particle` struct
- **rope** — establishes `DistanceConstraint` internal struct and XPBD projection pattern

### Required Modules
- **DiaMaths** — `Vector2D`, `sqrt`, `length()`
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `DIA_ASSERT`

### Dependent Features
- **soft-body-world** — creates and drives `Cloth` objects; calls `ProjectConstraints`, `CheckTearing`
- **geometry-collision** — iterates cloth particles for collision resolution
- **rigid-body-coupling** — iterates cloth particles for rigid body collision and coupling

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestCloth.cpp`)

1. **Particle count** — `GetParticleCount()` == `resX * resY`
2. **Grid spacing** — horizontal and vertical inter-particle distances match `width/(resX-1)` and `height/(resY-1)`
3. **GetParticle(x, y)** — correct position and invMass
4. **Out-of-range access** — `GetParticle(-1, 0)` triggers DIA_ASSERT in debug
5. **Structural constraints** — zero gravity, 10 steps: particles stay within expected distance
6. **Shear resistance** — shearStiffness=1 prevents diamond collapse under lateral displacement; shearStiffness=0 does not
7. **pinTopRow=true** — row y=0 particles all have invMass=0; do not move under gravity after steps
8. **PinParticle at runtime** — particle moves before pin, stays after pin
9. **UnpinParticle** — particle moves again after unpin
10. **No tearing when disabled** — `maxStretch=0`; stretch particles; `IsTorn()` stays false
11. **Tearing triggers** — `maxStretch=0.2`; over-stretch a pair; `CheckTearing()` removes constraint; `IsTorn()=true`
12. **IsTorn permanent** — set then stays true
13. **Assert on grid size** — `resX=64, resY=65` (4160 particles) triggers DIA_ASSERT

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for entity/component IDs | `kUniqueId` static constant + per-instance `mId` from `ClothDef::id` |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC<Particle>`, `DynamicArrayC<DistanceConstraint>`, `DynamicArrayC<float>` |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::SoftBody2D::` throughout |
| SD-001 | System | PBD solver | XPBD distance constraints across structural, shear, and bend layers |
| SD-004 | System | Tearing via `maxStretch` ratio (0 = disabled) | Per-constraint `active` flag; `mIsTorn` set permanently on first removal |
| SD-008 | System | No STL in public API | No STL in `ClothDef`, `Cloth`, or accessors |

## Status

`Approved`
