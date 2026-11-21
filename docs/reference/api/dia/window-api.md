# DiaWindow API

**Last Updated:** 2026-11-20

Window management API providing platform-agnostic window creation and management.

---

## Overview

**DiaWindow** provides a platform-agnostic interface for window creation and management.

**Location:** `Dia/DiaWindow/`

**Namespace:** `Dia::Window::`

**Key Concepts:**
- **IWindow** - Abstract window interface
- **IWindowFactory** - Creates and destroys windows
- **Platform-agnostic** - Backend implementation in DiaSDL (SDL3)
- **Window events** - Close, resize and focus changes arrive as input events (`Event::EType::kClosed`, `kResized`, `kLostFocus`, `kGainedFocus`) polled through `Dia::Input`, not through `IWindow`

**Implementation:** `Dia::SDL::Window` / `Dia::SDL::WindowFactory` (SDL3 backend, see [DiaSDL API](sdl-api.md))

---

## Core Interface

### IWindow

**Header:** `Dia/DiaWindow/Interface/IWindow.h`

**Purpose:** Abstract window interface implemented by the backend

#### Key Methods

```cpp
class IWindow
{
public:
    class Settings;   // title, Dimensions, Style

    virtual ~IWindow() {}

    // Lifecycle
    virtual void Initialize(const Settings& settings) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;

    // Geometry
    virtual Maths::Vector2D GetPosition() const = 0;
    virtual void SetPosition(const Maths::Vector2D& position) = 0;
    virtual Maths::Vector2D GetSize() const = 0;
    virtual void SetSize(const Maths::Vector2D& size) = 0;

    // Appearance
    virtual void SetTitle(const Core::Containers::String64& title) = 0;
    virtual void SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels) = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void SetMouseCursorVisible(bool visible) = 0;

    // Native handle
    virtual SystemHandle GetSystemHandle() const = 0;
};
```

### IWindow::Settings

```cpp
IWindow::Settings(const Core::Containers::String64& title,
                  const Dimensions& dimensions,
                  const Style& style);

IWindow::Settings::Dimensions(unsigned int width, unsigned int height,
                              unsigned int bitsPerPixel = 32);

// Style is a BitArray8 over EStyle: kTitlebar, kResize, kClose, kFullscreen.
// The default Style() enables kTitlebar, kResize and kClose.
IWindow::Settings::Style();
```

### IWindowFactory

**Header:** `Dia/DiaWindow/Interface/IWindowFactory.h`

```cpp
class IWindowFactory
{
public:
    virtual Window::IWindow* Create(const Window::IWindow::Settings& windowSetting) = 0;
    virtual void Destroy(Window::IWindow* window) = 0;
};
```

### SystemHandle

**Header:** `Dia/DiaWindow/SystemHandle.h`

`Dia::Window::SystemHandle` is the OS-specific native window handle: `HWND__*` on Windows, `void*` on Android, Linux and iOS. Renderers use it to attach to the window (see `AttachToNativeWindow` in DiaBgfx).

#### Usage Example

```cpp
Dia::SDL::WindowFactory factory;

Dia::Window::IWindow::Settings settings(
    "Game Title",
    Dia::Window::IWindow::Settings::Dimensions(1920, 1080),
    Dia::Window::IWindow::Settings::Style());

Dia::Window::IWindow* window = factory.Create(settings);

if (window->IsOpen())
{
    window->SetTitle("New Title");

    Dia::Maths::Vector2D size = window->GetSize();
    Dia::Window::SystemHandle handle = window->GetSystemHandle();
}

factory.Destroy(window);
```

---

## Backend Implementation

### DiaSDL Implementation

**Class:** `Dia::SDL::Window` (implements `IWindow` and `Dia::SDL::InputSource`)

**Factory:** `Dia::SDL::WindowFactory`

**Headers:** `Dia/DiaSDL/Window.h`, `Dia/DiaSDL/WindowFactory.h`

`WindowFactory::Create` initialises SDL3 (`SDL_INIT_VIDEO`) and returns a `Window`; `WindowFactory::Destroy` shuts SDL down. The window also acts as the input source, so the same object is registered with the input manager.

**[→ DiaSDL API Details](sdl-api.md)**

---

## Dependencies

**Required:**
- DiaCore (String64, BitArray8)
- DiaMaths (Vector2D)

**Implementation:**
- `Dia/DiaSDL/` - SDL3 backend

---

## Thread Safety

Window lifecycle and geometry methods must be called from the **main thread**, which owns the window and the kernel. Game logic runs on the simulation thread and communicates with the window through streams.

---

## Best Practices

- Create the window through an `IWindowFactory` and release it with the same factory.
- Check `IsOpen()` before use.
- React to `kClosed`, `kResized`, `kLostFocus` and `kGainedFocus` input events rather than polling window state.
- Use `GetSystemHandle()` to attach a renderer instead of reaching into backend types.

---

## Summary

**Interface:** `IWindow`, `IWindowFactory`, `SystemHandle` in `Dia/DiaWindow/`

**Backend:** DiaSDL (`Dia::SDL::Window`, `Dia::SDL::WindowFactory`)

**Lifecycle:**
- `Initialize(settings)` / `Close()` / `IsOpen()`

**Properties:**
- `GetPosition()` / `SetPosition()`
- `GetSize()` / `SetSize()`
- `SetTitle()` / `SetIcon()`
- `SetVisible()` / `SetMouseCursorVisible()`
- `GetSystemHandle()`

**Thread Safety:**
- Main thread only

**[→ API Overview](../api-overview.md)**  
**[→ DiaSDL API](sdl-api.md)**  
**[→ DiaGraphics API](graphics-api.md)**
