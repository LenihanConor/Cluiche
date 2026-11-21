# DiaSDL API

**Last Updated:** 2026-11-20

SDL3 window and input backend for the Dia engine.

---

## Overview

**Location:** `Dia/DiaSDL/`

**Namespace:** `Dia::SDL::`

**Purpose:** Implements `Dia::Window::IWindow`, `Dia::Window::IWindowFactory` and `Dia::Input::IInputSource` on top of SDL3. It is the window and input backend on all game targets. Rendering is not part of DiaSDL; the renderer (DiaBgfx / DiaBgfx3D) attaches to the window through `Window::GetSystemHandle()`.

**Headers:**
- `DiaSDL/Window.h`
- `DiaSDL/WindowFactory.h`
- `DiaSDL/InputSource.h`
- `DiaSDL/DisplayInfo.h`

**Not covered:** touch input and audio.

---

## Classes

### WindowFactory

```cpp
class WindowFactory : public Dia::Window::IWindowFactory
{
public:
    virtual Dia::Window::IWindow* Create(const Dia::Window::IWindow::Settings& windowSetting) override;
    virtual void Destroy(Dia::Window::IWindow* window) override;
};
```

`Create` calls `SDL_Init(SDL_INIT_VIDEO)` and returns a `Window`. `Destroy` releases the window and calls `SDL_Quit`. There is no separate SDL module or global init object.

### Window

```cpp
class Window : public Dia::Window::IWindow, public InputSource
{
public:
    void Initialize(const Dia::Window::IWindow::Settings& settings) override;
    void Close() override;
    bool IsOpen() const override;
    Maths::Vector2D GetPosition() const override;
    void SetPosition(const Maths::Vector2D& position) override;
    Maths::Vector2D GetSize() const override;
    void SetSize(const Maths::Vector2D& size) override;
    void SetTitle(const Core::Containers::String64& title) override;
    void SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels) override;
    void SetVisible(bool visible) override;
    void SetMouseCursorVisible(bool visible) override;
    Dia::Window::SystemHandle GetSystemHandle() const override;

protected:
    virtual void OnRawSDLEvent(const SDL_Event& event) override;  // hook, default no-op
};
```

The constructors are private; windows are created through `WindowFactory`. `Window` is both the window and its input source.

### InputSource

```cpp
class InputSource : public Dia::Input::IInputSource
{
public:
    // ESourceIndex / ESources: kSystem, kKeyboard, kMouse, kJoystick
    InputSource();

    void SetWindowContext(SDL_Window* window);
    void ListenForInputSources(Dia::Core::BitArray8 listeningToSource) override;
    void Poll(Dia::Input::EventData& outStream) override;

    static Dia::Input::EKey SDLKeycodeToEKey(SDL_Keycode key);
    static bool TranslateSDLEvent(const SDL_Event& sdlEvent, Dia::Input::Event& outEvent);

protected:
    virtual void OnRawSDLEvent(const SDL_Event& event) {}  // forward raw events, e.g. to ImGui
};
```

`Poll` drains `SDL_PollEvent` and appends translated `Dia::Input::Event` values to the output stream. Joystick axis values are normalised from the SDL range to -100..100. `SDLKeycodeToEKey` and `TranslateSDLEvent` are public so they can be unit tested directly.

### DisplayInfo

**Header:** `Dia/DiaSDL/DisplayInfo.h`

`Dia::SDL::GetPrimaryDisplaySize(unsigned int& outWidth, unsigned int& outHeight)` reports the primary display size (used by the kernel module to size the window).

---

## Usage

```cpp
Dia::SDL::WindowFactory windowFactory;

Dia::Window::IWindow::Settings settings(
    "Game",
    Dia::Window::IWindow::Settings::Dimensions(1280, 720),
    Dia::Window::IWindow::Settings::Style());

Dia::Window::IWindow* window = windowFactory.Create(settings);

// Choose which source categories to listen for
Dia::Core::BitArray8 mask;
mask.SetBit(Dia::SDL::InputSource::ESourceIndex::kSystem,   true);
mask.SetBit(Dia::SDL::InputSource::ESourceIndex::kKeyboard, true);
mask.SetBit(Dia::SDL::InputSource::ESourceIndex::kMouse,    true);
static_cast<Dia::SDL::Window*>(window)->ListenForInputSources(mask);

// Register the window as an input source
inputSourceManager.AddInputSource(static_cast<Dia::SDL::Window*>(window));

// Attach the renderer
canvas->AttachToNativeWindow(window->GetSystemHandle(), size);

// Shutdown
windowFactory.Destroy(window);
```

---

## Dependencies

- DiaCore, DiaWindow, DiaInput
- SDL3 (external)
- Must not depend on DiaBgfx or DiaGraphics

**Thread Safety:** main thread only.

**[→ DiaWindow API](window-api.md)**  
**[→ DiaInput API](input-api.md)**  
**[→ API Overview](../api-overview.md)**
