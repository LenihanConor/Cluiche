# Plan: drawer-boundary-consistency

**Spec:** @docs/specs/features/dia/diavisualdebugger/drawer-boundary-consistency.md
**Status:** In Progress

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `kGeoAABB`, `kGeoLabels`, `kScene2DOverview` to `DebugLayerNames.h` | Build clean | Pending | haiku | Three new `LayerNames` constants |
| 2 | Create `AABBOverlayDrawer` in `DiaGeometry2DVisualDebugger` | Build + existing geo tests pass | Pending | sonnet | Submit-per-frame pattern (like ShapeDrawer). `SubmitCircle/AARect/OORect/Line/Triangle/ConvexPoly/Capsule` each compute + buffer the AABB. `Draw()` flushes via `ShapeDrawer::SubmitAARect`. Namespace `Dia::Geometry2DVisualDebugger`. Layer: `kGeoAABB` |
| 3 | Create `ShapeLabelsDrawer` in `DiaGeometry2DVisualDebugger` | Build pass | Pending | sonnet | Submit-per-frame: `SubmitLabel(position, text)`. `Draw()` calls `RequestDrawText` for each. Namespace `Dia::Geometry2DVisualDebugger`. Layer: `kGeoLabels`. Has `DrawImGui` with font scale slider |
| 4 | Create `DiaScene2DVisualDebugger` module + `SceneOverviewDrawer` | Build pass | Pending | sonnet | New vcxproj + module doc. Shows camera positions (circle + FOV rect), light positions (circle + radius ring), layer bands. Does NOT render entities (that's entity-specific). Namespace `Dia::Scene2D`. Layer: `kScene2DOverview`. Depends on DiaCamera2D, DiaLighting2D, DiaScene2D, DiaGeometry2DVisualDebugger |
| 5 | Update `DiaGeometry2DVisualDebugger.vcxproj` with new files | Build pass | Pending | haiku | Add AABBOverlayDrawer.h/.cpp, ShapeLabelsDrawer.h/.cpp to vcxproj + filters |
| 6 | Add `DiaScene2DVisualDebugger.vcxproj` to `Cluiche.sln` | Build pass | Pending | haiku | New project in Dia visual debugger solution folder |
| 7 | Refactor CluicheTest `Geometry2DAABBDrawer` → thin wrapper | `dia run cluichetest` | Pending | sonnet | Submits its shape refs to engine `AABBOverlayDrawer` each frame. Remove AABB calculation logic. Keep layer name as CluicheTest-specific `"geometry2d.aabbs"` (test layer) |
| 8 | Refactor CluicheTest `Geometry2DLabelsDrawer` → thin wrapper | `dia run cluichetest` | Pending | sonnet | Submits labels to engine `ShapeLabelsDrawer`. Remove `RequestDrawText` calls. Keep ImGui and test-specific layout |
| 9 | Refactor CluicheTest `Scene2DTestDrawer` → consume `SceneOverviewDrawer` | `dia run cluichetest` | Pending | sonnet | Scene2DTestDrawer becomes thin: creates engine `SceneOverviewDrawer`, feeds registries. Entity rendering stays local (uses test-only `TransformComponent`). Rename file to `Scene2DTestDrawer` (no file rename needed — name is already correct for its CluicheTest role). Alternatively: remove entirely and register `SceneOverviewDrawer` directly from the stage, with entity circles as a separate CluicheTest-only drawer |
| 10 | Refactor `Animation2DTestDrawer` to delegate skeleton rendering | `dia run cluichetest` | Pending | sonnet | Replace hand-rolled bone lines + joint circles loop with engine `BoneLinesDrawer` + `JointCirclesDrawer` (already in `DiaRig2DVisualDebugger`). Keep test-specific ImGui (clip names, PASS/FAIL, wing angles). Keep dragon-specific colouring by constructing drawers with the computed `worldTransforms` |
| 11 | Naming consistency pass | Build clean | Pending | haiku | Verify: all engine drawers use `LayerNames::k*` (no inline StringCRC literals); CluicheTest drawers may use inline literals; no "Test" in engine drawer names; file names match `[Noun]Drawer` in their namespace context |
| 12 | Run full test suite | `dia run googletest` | Pending | haiku | All tests green |

## Design Decisions

### AABBOverlayDrawer API

```cpp
class AABBOverlayDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit AABBOverlayDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override; // kGeoAABB
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

    // Submit shapes — AABB computed internally. Buffer cleared each Draw().
    void SubmitCircle(const Dia::Geometry2D::Circle& shape);
    void SubmitAARect(const Dia::Geometry2D::AARect& shape);
    void SubmitOORect(const Dia::Geometry2D::OORect& shape);
    void SubmitLine(const Dia::Geometry2D::Line& shape);
    void SubmitTriangle(const Dia::Geometry2D::Triangle& shape);
    void SubmitConvexPoly(const Dia::Geometry2D::ConvexPolygon& shape);
    void SubmitCapsule(const Dia::Geometry2D::Capsule& shape);

private:
    static constexpr int kMaxAABBs = 64;
    struct AABBEntry { float minX, minY, maxX, maxY; };
    Dia::Core::Containers::DynamicArrayC<AABBEntry, kMaxAABBs> mPending;
    const Dia::Debug::DebugLayerManager& mManager;
};
```

### ShapeLabelsDrawer API

```cpp
class ShapeLabelsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit ShapeLabelsDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override; // kGeoLabels
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

    void SubmitLabel(Dia::Maths::Vector2D position, const char* text);

private:
    static constexpr int kMaxLabels = 64;
    struct LabelEntry { float x, y; const char* text; };
    Dia::Core::Containers::DynamicArrayC<LabelEntry, kMaxLabels> mPending;
    const Dia::Debug::DebugLayerManager& mManager;
    float mFontScale = 1.0f;
};
```

### SceneOverviewDrawer scope

Renders:
- Camera positions (circle at position, translucent rect for FOV)
- Lights (circle at position, radius ring)
- Layer bands (horizontal strips coloured by depth order)
- World bounds rect

Does NOT render entities — that crosses into entity-specific territory and the existing code depends on `TransformComponent` (a CluicheTest-only type).

### Animation2DTestDrawer refactor

Before:
```cpp
void Animation2DTestDrawer::Draw(...)
{
    // Hand-rolled skeleton rendering
    for (int i = 0; i < boneCount; ++i)
    {
        // compute world position
        // draw line to parent
        // draw joint circle
    }
}
```

After:
```cpp
void Animation2DTestDrawer::Draw(...)
{
    // Delegate to engine drawers
    Dia::Rig2D::BoneLinesDrawer boneLines(mSkeleton, mWorldTransforms, mLayerManager);
    Dia::Rig2D::JointCirclesDrawer jointCircles(mSkeleton, mWorldTransforms, mLayerManager);

    boneLines.Draw(frameData);
    jointCircles.Draw(frameData);
}
```

Note: The engine drawers use their own colour scheme (white bones, green/yellow/white joints). The test drawer currently uses custom colours (blue bones, green wings, yellow joints, red root). If the test needs custom colours, we either:
- Accept the engine's standard colours (preferred — removes duplication)
- Or keep the custom colour loop but still use ShapeDrawer for the primitives (less gain)

Recommendation: Accept engine colours. The dragon test is proving animation playback correctness, not testing specific colours.

### Naming rules enforced

| Context | Rule | Example |
|---------|------|---------|
| Engine drawer file | `[Noun]Drawer.h` — no domain prefix when namespace scopes it | `ShapeDrawer.h` in `Dia::Geometry2DVisualDebugger` |
| Engine drawer file (disambiguation needed) | `[DomainNoun]Drawer.h` | `PhysicsShapesDrawer.h` (because "ShapesDrawer" alone is ambiguous with geometry) |
| Engine layer name | `LayerNames::k*` constant from `DebugLayerNames.h` | `LayerNames::kGeoAABB` |
| CluicheTest drawer file | `[Domain][What]Drawer.h` or `[Domain]TestDrawer.h` | `Geometry2DAABBDrawer.h`, `Animation2DTestDrawer.h` |
| CluicheTest layer name | Inline `StringCRC("domain.purpose")` | `"geometry2d.aabbs"`, `"animation2d.dragon"` |
