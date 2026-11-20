# DiaGraphics API

**Last Updated:** 2026-11-20

Graphics abstraction API: the `ICanvas` rendering interface and the per-frame `FrameData` that producers fill and the renderer consumes.

---

## Overview

**DiaGraphics** defines how a frame is described and handed to a renderer. Game code does not draw directly; it records requests into a `FrameData`, and an `ICanvas` implementation renders that frame.

**Location:** `Dia/DiaGraphics/`

**Namespace:** `Dia::Graphics::`

**Key Concepts:**
- **ICanvas** - Abstract renderer interface (frame lifecycle)
- **FrameData** - Per-frame render requests (entity sprites, debug primitives, UI, CellField layers, camera)
- **Platform-agnostic** - Backend implementation in DiaBgfx (2D) and DiaBgfx3D (3D), both built on bgfx

**Implementation:** `Dia::Bgfx::Canvas` (bgfx backend), extended by `Dia::Bgfx3D::Canvas3D` for 3D

---

## Core Classes

### ICanvas

**Header:** `Dia/DiaGraphics/Interface/ICanvas.h`

**Purpose:** Abstract rendering interface implemented by the backend

```cpp
class ICanvas : public Dia::Observation::Capture::IFrameCaptureSource
{
public:
    class Settings;   // vsync, depth, stencil, antialiasing, GL version

    virtual void Initialize(const Settings& settings) = 0;
    virtual void SetCanvasSize(const Dia::Maths::Vector2D& size) = 0;
    virtual void Shutdown() {}

    virtual void StartFrame(const FrameData& nextFrame) = 0;
    virtual void ProcessFrame(const FrameData& nextFrame) = 0;
    virtual void EndFrame(const FrameData& nextFrame) = 0;

    // Convenience: StartFrame + ProcessFrame + EndFrame
    virtual void RenderFrame(const FrameData& frame);

    virtual FrameCaptureToken RequestFrameCapture();
    virtual FrameCaptureResult PollFrameCapture(const FrameCaptureToken& token);
    void ReleaseFrameCapture(const FrameCaptureToken& token) override;
};
```

### FrameData

**Header:** `Dia/DiaGraphics/Frame/FrameData.h`

`FrameData` aggregates `DebugFrameData`, `UIFrameData`, `EntityFrameData` and `CellFieldFrameData`, plus the camera, window size and mouse pixel.

```cpp
class FrameData : public DebugFrameData, public UIFrameData,
                  public EntityFrameData, public CellFieldFrameData
{
public:
    void Clear();
    void Copy(const FrameData& rhs);

    const Dia::Camera2D::Camera2D& GetCamera() const;
    void SetCamera(const Dia::Camera2D::Camera2D& camera);

    const Dia::Maths::Vector2D& GetWindowSize() const;
    void SetWindowSize(const Dia::Maths::Vector2D& size);

    const Dia::Maths::Vector2D& GetMousePixel() const;
    void SetMousePixel(const Dia::Maths::Vector2D& pixel);
};
```

**Producers record requests:**
- `EntityFrameData::RequestDrawSprite(const SpriteDrawCommand& cmd)`
- `DebugFrameData::RequestDraw(...)` (circles, lines, triangles), `RequestDrawPoint`, `RequestDrawRect`, `RequestDrawArc`, `RequestDrawRay`, `RequestDrawText`
- `DebugFrameData::RequestDrawLine3D`, `RequestDrawRay3D`, `RequestDrawBox3D`, `RequestDrawSphere3D`
- `CellFieldFrameData` / `FrameData::RequestDrawCellField(const Dia::Core::CellFieldDrawRequest&)`

### RGBA

**Header:** `Dia/DiaGraphics/Misc/RGBA.h`

Colour type used by the debug primitives (`RGBA(0, 0, 0, 0)` is used as "no fill").

---

## Common Patterns

### Render a Frame

```cpp
Dia::Graphics::FrameData frame;
frame.SetCamera(camera);
frame.SetWindowSize(windowSize);

frame.RequestDrawSprite(spriteCommand);
frame.RequestDrawPoint(position, colour);

canvas->RenderFrame(frame);   // StartFrame + ProcessFrame + EndFrame
```

### Coordinate System

- Origin: top-left of the canvas
- X: left to right
- Y: top to bottom

---

## Backend Implementation

### DiaBgfx Implementation

**Class:** `Dia::Bgfx::Canvas` (implements `ICanvas`)

**Header:** `Dia/DiaBgfx/Canvas.h`

```cpp
class Canvas : public Dia::Graphics::ICanvas
{
public:
    // Must be called BEFORE Initialize
    void AttachToNativeWindow(Dia::Window::SystemHandle hwnd,
                              const Dia::Maths::Vector2D& size);

    void Initialize(const Dia::Graphics::ICanvas::Settings& settings) override;
    void Shutdown() override;
    void SetCanvasSize(const Dia::Maths::Vector2D& size) override;
    void StartFrame(const Dia::Graphics::FrameData& nextFrame) override;
    void ProcessFrame(const Dia::Graphics::FrameData& nextFrame) override;
    void EndFrame(const Dia::Graphics::FrameData& nextFrame) override;

    Dia::UI::IUIRenderOverlay* GetUIRenderOverlay();
    void SetCellFieldRegistry(Dia::Graphics::CellFieldRegistry* registry);
};
```

Configuration is passed through `Dia::Bgfx::CanvasSettings` (`rendererType`: Direct3D11, Direct3D12 or Vulkan; `initialSize`; `cookedShaderRoot`). bgfx calls must come from the thread that called `bgfx::init()`, so `Initialize` stores settings and the real initialisation is deferred to the first `StartFrame` on the render processing unit.

`Dia::Bgfx3D::Canvas3D` (`Dia/DiaBgfx3D/`) extends `Dia::Bgfx::Canvas` and renders 3D passes before the inherited 2D passes.

The window itself comes from DiaSDL, not from the canvas ([DiaWindow API](window-api.md), [DiaSDL API](sdl-api.md)).

---

## Dependencies

**Required:**
- `Dia/DiaMaths/` - Vector2D
- `Dia/DiaCamera2D/` - Camera2D

**Implementation:**
- `Dia/DiaBgfx/` - 2D bgfx backend
- `Dia/DiaBgfx3D/` - 3D bgfx backend

---

## Thread Safety

- Frame requests are recorded by producers and handed to the render processing unit; `ICanvas` methods run on the render thread only.

---

## Best Practices

- Record into `FrameData`; never call the backend from game logic.
- Call `AttachToNativeWindow` before `Initialize`.
- Use `RenderFrame` unless you need to interleave work between the three frame phases.

---

## Summary

**Interface:** `ICanvas`, `FrameData` in `Dia/DiaGraphics/`

**Backend:** DiaBgfx (`Dia::Bgfx::Canvas`), DiaBgfx3D (`Dia::Bgfx3D::Canvas3D`)

**Thread Safety:**
- Render thread only

**[→ API Overview](../api-overview.md)**  
**[→ DiaWindow API](window-api.md)**  
**[→ DiaMaths API](maths-api.md)**
