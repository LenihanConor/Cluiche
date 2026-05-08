# Feature Spec: debug-text-primitive

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Adds a `Text2D` variant to the `DebugPrimitive` tagged union, enabling world-space text labels to be submitted via `DebugFrameData::RequestDrawText()` and rendered by the `DiaSFML` visitor using `sf::Text` with SFML's built-in default font. Unlocks bone name labels, entity ID overlays, IK chain identifiers, and any other world-space label drawn at a position in the scene by current and future debug draw classes.

**Problem solved:** There is no mechanism to draw text in world space through the `DebugPrimitive` system today. Bone names, IK chain IDs, and convergence state labels are all impossible without this. ImGui (DiaVisualDebuggerConsole) handles screen-space overlay text but cannot draw text at a world-space position registered to a simulation object.

---

## Acceptance Criteria

1. `DebugPrimitiveType::Text2D` added as enum value `7`
2. `DebugPrimitiveText2D` struct contains: world-space `position`, fixed-length `char text[64]`, `fontSize` float, `RGBA colour`
3. `DebugPrimitive` union includes `text2D` member; `operator=` and copy constructor handle `Text2D` case
4. `DebugFrameData::RequestDrawText()` submits a `Text2D` primitive (subject to `CanAdd()` guard from `debug-budget`)
5. `DebugFrameRendererVisitor` renders `Text2D` using `sf::Text` with SFML's built-in default font (`sf::Font::getDefaultFont()` / default-constructed `sf::Text`)
6. Text is rendered at the world-space `position` directly — no camera transform applied by the primitive system (draw classes are responsible for passing the correct world position)
7. Strings longer than 63 characters are silently truncated (null terminator always guaranteed)
8. `fontSize` of 0.0f or negative is treated as a no-op (no draw, no crash)
9. `DebugFrameData` remains trivially assignable — `char[64]` is a trivially copyable type
10. All existing `RequestDraw*` call sites and tests unchanged

---

## Design

### DebugPrimitiveText2D struct

```cpp
struct DebugPrimitiveText2D
{
    Maths::Vector2D position;
    char            text[64];   // null-terminated; truncated silently if source > 63 chars
    float           fontSize;
    RGBA            colour;
};
```

Fixed-length `char[64]` — no `std::string`, no heap allocation (PD-004, SD-DBG-012). The 64-byte limit covers all anticipated debug labels (bone names, chain IDs, state names). Truncation is silent and safe — the null terminator at `text[63]` is always enforced by `RequestDrawText()`.

### DebugPrimitiveType enum update

```cpp
enum class DebugPrimitiveType : unsigned char
{
    Circle2D   = 0,
    Line2D     = 1,
    Point2D    = 2,
    Rect2D     = 3,
    Arc2D      = 4,
    Ray2D      = 5,
    Triangle2D = 6,
    Text2D     = 7   // new
};
```

### DebugPrimitive union and operator= update

```cpp
struct DebugPrimitive
{
    DebugPrimitiveType type;
    uint32_t           entityId = 0;  // picking seam (from debug-budget)

    union
    {
        DebugPrimitiveCircle2D   circle2D;
        DebugPrimitiveLine2D     line2D;
        DebugPrimitivePoint2D    point2D;
        DebugPrimitiveRect2D     rect2D;
        DebugPrimitiveArc2D      arc2D;
        DebugPrimitiveRay2D      ray2D;
        DebugPrimitiveTriangle2D triangle2D;
        DebugPrimitiveText2D     text2D;    // new
    };

    DebugPrimitive() : type(DebugPrimitiveType::Circle2D), entityId(0), circle2D() {}
    DebugPrimitive(const DebugPrimitive& rhs) { *this = rhs; }
    DebugPrimitive& operator=(const DebugPrimitive& rhs)
    {
        type     = rhs.type;
        entityId = rhs.entityId;
        switch (type)
        {
            case DebugPrimitiveType::Circle2D:   circle2D   = rhs.circle2D;   break;
            case DebugPrimitiveType::Line2D:     line2D     = rhs.line2D;     break;
            case DebugPrimitiveType::Point2D:    point2D    = rhs.point2D;    break;
            case DebugPrimitiveType::Rect2D:     rect2D     = rhs.rect2D;     break;
            case DebugPrimitiveType::Arc2D:      arc2D      = rhs.arc2D;      break;
            case DebugPrimitiveType::Ray2D:      ray2D      = rhs.ray2D;      break;
            case DebugPrimitiveType::Triangle2D: triangle2D = rhs.triangle2D; break;
            case DebugPrimitiveType::Text2D:     text2D     = rhs.text2D;     break;
        }
        return *this;
    }
};
```

Note: `DebugPrimitiveText2D` contains `char[64]` which is trivially copyable — the compiler-generated copy for `text2D = rhs.text2D` is correct. No explicit `memcpy` required.

### RequestDrawText in DebugFrameData

```cpp
// DebugFrameData.h
void RequestDrawText(const Maths::Vector2D& position,
                     const char* text,
                     float fontSize,
                     RGBA colour);

// DebugFrameData.cpp
void DebugFrameData::RequestDrawText(const Maths::Vector2D& position,
                                     const char* text,
                                     float fontSize,
                                     RGBA colour)
{
    if (!CanAdd()) return;
    if (fontSize <= 0.0f) return;

    DebugPrimitive p;
    p.type               = DebugPrimitiveType::Text2D;
    p.text2D.position    = position;
    p.text2D.fontSize    = fontSize;
    p.text2D.colour      = colour;

    // Safe truncating copy — null terminator always at [63]
    unsigned int i = 0;
    if (text != nullptr)
    {
        for (; i < 63 && text[i] != '\0'; ++i)
            p.text2D.text[i] = text[i];
    }
    p.text2D.text[i] = '\0';

    mDebugPrimitiveBuffer.Add(p);
}
```

### DiaSFML renderer — DrawText2D

SFML's default font is available via `sf::Font::getDefaultFont()` (SFML 2.x) which returns a reference to an internal font with no file loading required.

```cpp
// DebugFrameRendererVisitor.h — add to private section
void DrawText2D(const Dia::Graphics::DebugPrimitiveText2D& p) const;

// DebugFrameRendererVisitor.cpp — switch case
case Dia::Graphics::DebugPrimitiveType::Text2D: DrawText2D(p.text2D); break;

// DebugFrameRendererVisitor.cpp — implementation
void DebugFrameRendererVisitor::DrawText2D(const Dia::Graphics::DebugPrimitiveText2D& p) const
{
    sf::Text sfText;
    sfText.setFont(sf::Font::getDefaultFont());
    sfText.setString(p.text);
    sfText.setCharacterSize(static_cast<unsigned int>(p.fontSize));
    sfText.setPosition(sf::Vector2f(p.position.x, p.position.y));

    sf::Color sfColour;
    Convert(sfColour, p.colour);
    sfText.setFillColor(sfColour);

    mRenderTarget->draw(sfText);
}
```

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaGraphics/Frame/DebugPrimitive.h` | Add `Text2D = 7` to enum; add `DebugPrimitiveText2D` struct; add `text2D` to union; update `operator=` |
| `Dia/DiaGraphics/Frame/DebugFrameData.h` | Add `RequestDrawText()` declaration |
| `Dia/DiaGraphics/Frame/DebugFrameData.cpp` | Add `RequestDrawText()` implementation |
| `Dia/DiaSFML/DebugFrameRendererVisitor.h` | Add `DrawText2D()` private declaration |
| `Dia/DiaSFML/DebugFrameRendererVisitor.cpp` | Add `Text2D` switch case; add `DrawText2D()` implementation |

**No other files change.** Existing draw classes, tests, and `FrameData.h/.cpp` are untouched.

**Prerequisite:** `debug-budget` must be implemented first — `RequestDrawText()` depends on `CanAdd()`.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `DebugPrimitiveText2D` struct and `Text2D` enum value to `DebugPrimitive.h`; update union, constructor, `operator=` | `DebugPrimitive.h` |
| 2 | Add `RequestDrawText()` declaration to `DebugFrameData.h` | `.h` |
| 3 | Implement `RequestDrawText()` in `DebugFrameData.cpp` with safe truncating copy | `.cpp` |
| 4 | Add `DrawText2D()` declaration to `DebugFrameRendererVisitor.h` | `.h` |
| 5 | Add `Text2D` case to visitor switch; implement `DrawText2D()` in `DebugFrameRendererVisitor.cpp` | `.cpp` |
| 6 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 7 | Write tests — see test plan | `TestDebugTextPrimitive.cpp` in GoogleTests |
| 8 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaGraphics/TestDebugTextPrimitive.cpp`

| Suite | Test | What it verifies |
|-------|------|-----------------|
| Submission | `RequestDrawText_ProducesPrimitive` | Submit one text → `GetDebugPrimitiveCount() == 1` |
| Submission | `RequestDrawText_TypeIsText2D` | Submitted primitive → `type == DebugPrimitiveType::Text2D` |
| Submission | `RequestDrawText_PositionPreserved` | Submit at (10, 20) → `text2D.position == (10, 20)` |
| Submission | `RequestDrawText_ColourPreserved` | Submit with cyan → `text2D.colour == DebugColourPalette::kGoal` |
| Submission | `RequestDrawText_FontSizePreserved` | Submit fontSize=14 → `text2D.fontSize == 14.0f` |
| Submission | `RequestDrawText_ShortString` | Submit "hello" → `text2D.text == "hello"` |
| Submission | `RequestDrawText_MaxLengthString` | Submit 63-char string → stored exactly, null at [63] |
| Submission | `RequestDrawText_OverLengthTruncated` | Submit 80-char string → stored as first 63 chars, null at [63] |
| Submission | `RequestDrawText_NullptrText` | Submit nullptr → primitive stored with empty string, no crash |
| Submission | `RequestDrawText_ZeroFontSize_NoPrimitive` | fontSize=0.0f → `GetDebugPrimitiveCount() == 0` |
| Submission | `RequestDrawText_NegativeFontSize_NoPrimitive` | fontSize=-1.0f → `GetDebugPrimitiveCount() == 0` |
| Submission | `RequestDrawText_ObeysCapacityGuard` | Fill buffer to capacity then submit text → `DroppedCount() == 1` (requires debug-budget) |
| Copy | `Text2D_CopyConstruct` | Copy a text primitive → all fields identical in copy |
| Copy | `Text2D_AssignmentOp` | Assign a text primitive → all fields identical in destination |
| Copy | `Text2D_StringCopied` | Copy primitive with "test" string → copy has identical string content |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — no string IDs introduced |
| PD-002 | ProcessingUnit/Phase/Module architecture | Compliant — data type change only |
| PD-003 | Component-based entities | Compliant — `entityId` seam from `debug-budget` carries through |
| PD-004 | No STL in public APIs | Compliant — `char[64]` fixed buffer, not `std::string`; `RequestDrawText` takes `const char*` |
| PD-005 | x64 only | Compliant — no platform-specific code |
| PD-006 | Visual Studio project files are source of truth | Compliant — no new projects; existing `DiaGraphics.vcxproj` and `DiaSFML.vcxproj` include these files already |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant — no project file changes |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — no generated output |
| AD-001 | Module YAML frontmatter | Compliant — `dia.graphics.architecture.module.md` updated to reflect new `RequestDrawText()` entry point; `dia.sfml.architecture.module.md` updated to reflect `DrawText2D` |
| AD-002 | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | `Dia::<Module>::` namespace | Compliant — DiaGraphics changes in `Dia::Graphics::`, DiaSFML changes in `Dia::SFML::` |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards | Compliant — `RequestDrawText()` is only called from debug-only draw classes; no guard needed inside `DebugFrameData` itself (excluded at vcxproj level in Release) |
| SD-DBG-009 | `DebugFrameData` must remain trivially copyable | Compliant — `char[64]` is trivially copyable; `DebugPrimitive` copy assignment handles `Text2D` case explicitly; no heap allocation |
| SD-DBG-012 | `TextPrimitive` uses `char[64]` not `std::string` | Compliant — this feature implements the constraint exactly as specified |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | SFML default font | `sf::Font::getDefaultFont()` is available in SFML 2.x. Is this the SFML version in use? | SFML is in `External/SFML/` — confirmed 2.x based on existing usage of `sf::PrimitiveType::Lines` and `sf::ConvexShape`. `getDefaultFont()` is available. |
| 2 | Text coordinate space | `RequestDrawText()` takes a world-space position. Does the SFML render target use world-space coordinates directly (no camera transform applied by the renderer)? | The `DebugFrameRendererVisitor` draws directly into the SFML render target using the coordinates as-is — the same as all other debug primitives. Draw classes are responsible for passing the correct world position. This is consistent with all other `RequestDraw*` calls. |
| 3 | Font size units | `fontSize` is passed to `sf::Text::setCharacterSize(unsigned int)` which takes pixel size. Should `fontSize` be documented as pixels? | Yes — `fontSize` is in pixels matching SFML's `setCharacterSize()`. Draw classes should use values like 12.0f–16.0f for typical debug labels. Document this in the API comment. |
| 4 | String encoding | `char[64]` is ASCII. Are all anticipated debug labels (bone names, chain IDs, state names) ASCII-safe? | Yes — all Dia identifiers are ASCII `StringCRC` values; bone names and chain IDs are defined in code as string literals. No multi-byte encoding needed. |
| 5 | Text origin alignment | `sf::Text` by default places the text top-left at `position`. Should it be centred? | Top-left is correct for debug labels — they read left-to-right from the labelled point. Centring would require measuring text bounds per draw, which is unnecessary overhead. Document as top-left origin in API comment. |

---

## Status

`Done`
