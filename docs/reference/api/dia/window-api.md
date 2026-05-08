# DiaWindow API

**Last Updated:** 2026-04-01

Window management API providing platform-agnostic window creation and event handling.

---

## Overview

**DiaWindow** provides a platform-agnostic interface for window creation, management, and event handling.

**Location:** `Dia/DiaWindow/`

**Namespace:** `Dia::Window::`

**Key Concepts:**
- **IWindow** - Abstract window interface
- **Platform-agnostic** - Backend implementation in DiaSFML
- **Event handling** - Window events (resize, close, focus)

**Implementation:** `Dia::SFML::DiaSFMLRenderWindow` (SFML backend)

---

## Core Interface

### IWindow

**Header:** `Dia/DiaWindow/Interface/IWindow.h`

**Purpose:** Abstract window interface implemented by backend

#### Key Methods

```cpp
class IWindow
{
public:
    virtual ~IWindow() = default;
    
    // Lifecycle
    virtual void Create(unsigned int width, unsigned int height, const char* title) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
    
    // Display
    virtual void Display() = 0;
    virtual void Clear() = 0;
    
    // Properties
    virtual unsigned int GetWidth() const = 0;
    virtual unsigned int GetHeight() const = 0;
    virtual void SetTitle(const char* title) = 0;
    
    // Fullscreen
    virtual void SetFullscreen(bool fullscreen) = 0;
    virtual bool IsFullscreen() const = 0;
    
    // Focus
    virtual bool HasFocus() const = 0;
    virtual void RequestFocus() = 0;
    
    // VSync
    virtual void SetVSync(bool enabled) = 0;
    
    // Frame rate
    virtual void SetFramerateLimit(unsigned int limit) = 0;
};
```

#### Usage Example

```cpp
// Create window
Dia::Window::IWindow* window = CreateWindow();  // Backend-specific
window->Create(1920, 1080, "Game Title");

// Check if open
if (window->IsOpen())
{
    // Clear and display
    window->Clear();
    DrawFrame();
    window->Display();
}

// Set properties
window->SetTitle("New Title");
window->SetFullscreen(true);
window->SetVSync(true);
window->SetFramerateLimit(60);

// Query properties
unsigned int width = window->GetWidth();
unsigned int height = window->GetHeight();
bool hasFocus = window->HasFocus();

// Close
window->Close();
```

---

## Common Patterns

### Window Creation and Main Loop

```cpp
class WindowModule : public Dia::Application::Module
{
public:
    void OnConstruct() override
    {
        mWindow = CreateWindow();  // Backend-specific
    }
    
    void OnStart() override
    {
        mWindow->Create(1920, 1080, "Game");
        mWindow->SetVSync(true);
        mWindow->SetFramerateLimit(60);
    }
    
    void OnUpdate() override
    {
        if (!mWindow->IsOpen())
        {
            // Window closed, exit application
            QueuePhaseTransition(Dia::Application::Phase::kShutdown);
            return;
        }
        
        mWindow->Clear();
        
        // Render happens here via other modules
        
        mWindow->Display();
    }
    
    void OnStop() override
    {
        mWindow->Close();
    }
    
    void OnDestruct() override
    {
        delete mWindow;
        mWindow = nullptr;
    }
    
private:
    Dia::Window::IWindow* mWindow;
};
```

---

### Fullscreen Toggle

```cpp
class WindowManager
{
public:
    void ToggleFullscreen()
    {
        bool isFullscreen = mWindow->IsFullscreen();
        mWindow->SetFullscreen(!isFullscreen);
        
        // May need to recreate window for some backends
        if (!isFullscreen)
        {
            // Now fullscreen
            mWindow->SetTitle("Game (Fullscreen)");
        }
        else
        {
            // Now windowed
            mWindow->SetTitle("Game");
        }
    }
    
private:
    Dia::Window::IWindow* mWindow;
};
```

---

### Window Resize Handling

```cpp
class RenderModule : public Dia::Application::Module
{
public:
    void OnUpdate() override
    {
        // Check if window size changed
        unsigned int newWidth = mWindow->GetWidth();
        unsigned int newHeight = mWindow->GetHeight();
        
        if (newWidth != mCachedWidth || newHeight != mCachedHeight)
        {
            OnWindowResized(newWidth, newHeight);
            mCachedWidth = newWidth;
            mCachedHeight = newHeight;
        }
    }
    
private:
    void OnWindowResized(unsigned int width, unsigned int height)
    {
        // Update viewport
        UpdateViewport(width, height);
        
        // Update camera aspect ratio
        mCamera->SetAspectRatio((float)width / (float)height);
        
        // Notify other systems
        NotifyWindowResized(width, height);
    }
    
    Dia::Window::IWindow* mWindow;
    unsigned int mCachedWidth = 0;
    unsigned int mCachedHeight = 0;
};
```

---

### VSync and Frame Rate Control

```cpp
class RenderSettings
{
public:
    void SetVSync(bool enabled)
    {
        mWindow->SetVSync(enabled);
        
        if (enabled)
        {
            // VSync enabled, remove frame rate limit
            mWindow->SetFramerateLimit(0);
        }
        else
        {
            // VSync disabled, set frame rate limit
            mWindow->SetFramerateLimit(60);
        }
    }
    
    void SetFrameRateLimit(unsigned int fps)
    {
        if (mWindow->IsFullscreen())
        {
            // Use VSync in fullscreen
            mWindow->SetVSync(true);
            mWindow->SetFramerateLimit(0);
        }
        else
        {
            // Use frame rate limit in windowed mode
            mWindow->SetVSync(false);
            mWindow->SetFramerateLimit(fps);
        }
    }
    
private:
    Dia::Window::IWindow* mWindow;
};
```

---

### Focus Management

```cpp
class FocusManager
{
public:
    void OnUpdate()
    {
        bool hasFocus = mWindow->HasFocus();
        
        if (hasFocus != mWasFocused)
        {
            if (hasFocus)
            {
                OnFocusGained();
            }
            else
            {
                OnFocusLost();
            }
            mWasFocused = hasFocus;
        }
    }
    
private:
    void OnFocusGained()
    {
        // Resume game
        mGamePaused = false;
        mAudioMuted = false;
    }
    
    void OnFocusLost()
    {
        // Pause game
        mGamePaused = true;
        mAudioMuted = true;
    }
    
    Dia::Window::IWindow* mWindow;
    bool mWasFocused = true;
    bool mGamePaused = false;
    bool mAudioMuted = false;
};
```

---

## Backend Implementation

### DiaSFML Implementation

**Class:** `Dia::SFML::DiaSFMLRenderWindow`

**Header:** `Dia/DiaSFML/DiaSFMLRenderWindow.h`

```cpp
class DiaSFMLRenderWindow : public Dia::Window::IWindow
{
public:
    DiaSFMLRenderWindow();
    
    // IWindow implementation
    void Create(unsigned int width, unsigned int height, const char* title) override;
    void Close() override;
    bool IsOpen() const override;
    
    void Display() override;
    void Clear() override;
    
    unsigned int GetWidth() const override;
    unsigned int GetHeight() const override;
    void SetTitle(const char* title) override;
    
    void SetFullscreen(bool fullscreen) override;
    bool IsFullscreen() const override;
    
    bool HasFocus() const override;
    void RequestFocus() override;
    
    void SetVSync(bool enabled) override;
    void SetFramerateLimit(unsigned int limit) override;
    
    // SFML-specific
    sf::RenderWindow& GetSFMLWindow();
    
private:
    sf::RenderWindow mWindow;
    bool mIsFullscreen;
};
```

**[→ DiaSFML API Details](sfml-api.md)**

---

## Dependencies

**Required:**
- None (self-contained interface)

**Implementation:**
- `Dia/DiaSFML/` - SFML backend

---

## Thread Safety

| Method | Thread Safety |
|--------|---------------|
| `Create/Close` | ❌ Call from Main thread only |
| `Display/Clear` | ❌ Call from Main thread only |
| `GetWidth/GetHeight` | ✅ Thread-safe (read-only) |
| `SetTitle` | ❌ Call from Main thread only |
| `SetFullscreen` | ❌ Call from Main thread only |
| `IsOpen/HasFocus` | ✅ Thread-safe (read-only) |

**Important:** Window lifecycle and rendering methods must be called from the **Main thread**.

---

## Best Practices

### 1. Clear Before Rendering

```cpp
// ✅ Good: Clear each frame
window->Clear();
DrawFrame();
window->Display();

// ❌ Bad: No clear (artifacts from previous frames)
DrawFrame();
window->Display();
```

---

### 2. Display After Rendering

```cpp
// ✅ Good: Display after all rendering
window->Clear();
DrawBackground();
DrawGame();
DrawUI();
window->Display();

// ❌ Bad: Multiple displays (flicker)
window->Clear();
DrawBackground();
window->Display();
DrawGame();
window->Display();
```

---

### 3. Check IsOpen Before Use

```cpp
// ✅ Good: Check before use
if (window->IsOpen())
{
    window->Clear();
    DrawFrame();
    window->Display();
}

// ❌ Bad: No check (crash if closed)
window->Clear();
DrawFrame();
window->Display();
```

---

### 4. Handle Focus Loss

```cpp
// ✅ Good: Pause on focus loss
void OnUpdate()
{
    if (!window->HasFocus())
    {
        PauseGame();
        return;  // Skip update
    }
    
    UpdateGame();
}

// ❌ Bad: Continue running without focus
void OnUpdate()
{
    UpdateGame();  // Runs even when minimized
}
```

---

## Gotchas

### Gotcha 1: SetFullscreen May Recreate Window

Some backends **recreate the window** when toggling fullscreen, losing context:

```cpp
// ⚠️ Warning: Resources may be invalidated
window->SetFullscreen(true);

// May need to reload resources
ReloadTextures();
ReloadShaders();
```

---

### Gotcha 2: Display Is Blocking

`Display()` may **block** waiting for VSync:

```cpp
// If VSync enabled at 60 FPS, this blocks for ~16ms
window->Display();
```

This is intentional (frame pacing), but be aware for timing-sensitive code.

---

### Gotcha 3: Width/Height During Resize

During resize, `GetWidth()/GetHeight()` may return **old values** until the event is processed:

```cpp
// ❌ May be out of sync during resize
unsigned int width = window->GetWidth();
unsigned int height = window->GetHeight();

// ✅ Better: Cache on resize event
void OnWindowResized(unsigned int width, unsigned int height)
{
    mCachedWidth = width;
    mCachedHeight = height;
}
```

---

### Gotcha 4: Frame Rate Limit vs VSync

**Don't enable both** frame rate limit and VSync:

```cpp
// ❌ Bad: Both enabled (conflicts)
window->SetVSync(true);
window->SetFramerateLimit(60);

// ✅ Good: One or the other
window->SetVSync(true);
window->SetFramerateLimit(0);  // Disable limit

// OR
window->SetVSync(false);
window->SetFramerateLimit(60);
```

---

## Limitations

### Current Limitations

1. **Single window** - Only one window supported
2. **No multi-monitor control** - Can't choose display
3. **Limited fullscreen modes** - No borderless windowed mode
4. **No window decorations control** - Can't hide titlebar
5. **No transparency** - No alpha channel for window

### Future Improvements

- Multi-window support
- Multi-monitor API
- Borderless windowed mode
- Window decorations control
- Per-pixel transparency

**[→ Future Directions](../../design-rationale/future-directions.md)**

---

## Examples

### Full Window Management Module

```cpp
class WindowModule : public Dia::Application::Module
{
public:
    WindowModule()
        : mWindow(nullptr)
        , mCachedWidth(0)
        , mCachedHeight(0)
        , mWasFocused(true)
    {}
    
    void OnConstruct() override
    {
        mWindow = new Dia::SFML::DiaSFMLRenderWindow();
    }
    
    void OnStart() override
    {
        // Create window
        mWindow->Create(1920, 1080, "Cluiche");
        
        // Configure
        mWindow->SetVSync(true);
        mWindow->SetFramerateLimit(0);  // VSync controls frame rate
        
        // Cache initial size
        mCachedWidth = mWindow->GetWidth();
        mCachedHeight = mWindow->GetHeight();
    }
    
    void OnUpdate() override
    {
        // Check if window closed
        if (!mWindow->IsOpen())
        {
            QueuePhaseTransition(Dia::Application::Phase::kShutdown);
            return;
        }
        
        // Handle resize
        CheckResize();
        
        // Handle focus
        CheckFocus();
        
        // Render
        mWindow->Clear();
        // Rendering happens via other modules
        mWindow->Display();
    }
    
    void OnStop() override
    {
        mWindow->Close();
    }
    
    void OnDestruct() override
    {
        delete mWindow;
        mWindow = nullptr;
    }
    
    Dia::Window::IWindow* GetWindow() const { return mWindow; }
    
private:
    void CheckResize()
    {
        unsigned int width = mWindow->GetWidth();
        unsigned int height = mWindow->GetHeight();
        
        if (width != mCachedWidth || height != mCachedHeight)
        {
            OnWindowResized(width, height);
            mCachedWidth = width;
            mCachedHeight = height;
        }
    }
    
    void CheckFocus()
    {
        bool hasFocus = mWindow->HasFocus();
        
        if (hasFocus != mWasFocused)
        {
            if (hasFocus)
            {
                OnFocusGained();
            }
            else
            {
                OnFocusLost();
            }
            mWasFocused = hasFocus;
        }
    }
    
    void OnWindowResized(unsigned int width, unsigned int height)
    {
        // Notify other systems
        NotifyResize(width, height);
    }
    
    void OnFocusGained()
    {
        // Resume
    }
    
    void OnFocusLost()
    {
        // Pause
    }
    
    Dia::Window::IWindow* mWindow;
    unsigned int mCachedWidth;
    unsigned int mCachedHeight;
    bool mWasFocused;
};
```

---

### Settings Management

```cpp
class WindowSettings
{
public:
    struct Settings
    {
        unsigned int width = 1920;
        unsigned int height = 1080;
        bool fullscreen = false;
        bool vsync = true;
        unsigned int framerateLimit = 60;
    };
    
    void Apply(Dia::Window::IWindow* window, const Settings& settings)
    {
        // Apply fullscreen
        window->SetFullscreen(settings.fullscreen);
        
        // Apply VSync
        window->SetVSync(settings.vsync);
        
        // Apply frame rate limit (if VSync disabled)
        if (!settings.vsync)
        {
            window->SetFramerateLimit(settings.framerateLimit);
        }
        else
        {
            window->SetFramerateLimit(0);
        }
    }
    
    void SaveSettings(const Settings& settings)
    {
        // Save to config file
        JsonWriter json;
        json.WriteInt("width", settings.width);
        json.WriteInt("height", settings.height);
        json.WriteBool("fullscreen", settings.fullscreen);
        json.WriteBool("vsync", settings.vsync);
        json.WriteInt("framerateLimit", settings.framerateLimit);
        json.Save("config/window.json");
    }
    
    Settings LoadSettings()
    {
        Settings settings;
        
        JsonReader json;
        if (json.Load("config/window.json"))
        {
            settings.width = json.ReadInt("width", 1920);
            settings.height = json.ReadInt("height", 1080);
            settings.fullscreen = json.ReadBool("fullscreen", false);
            settings.vsync = json.ReadBool("vsync", true);
            settings.framerateLimit = json.ReadInt("framerateLimit", 60);
        }
        
        return settings;
    }
};
```

---

## Summary

**Core Interface:**
- `IWindow` - Abstract window interface

**Lifecycle:**
- `Create(width, height, title)` - Create window
- `Close()` - Close window
- `IsOpen()` - Check if open

**Display:**
- `Clear()` - Clear frame buffer
- `Display()` - Present frame (may block for VSync)

**Properties:**
- `GetWidth()/GetHeight()` - Query size
- `SetTitle(title)` - Change title

**Fullscreen:**
- `SetFullscreen(bool)` - Toggle fullscreen
- `IsFullscreen()` - Query fullscreen state

**Focus:**
- `HasFocus()` - Check focus state
- `RequestFocus()` - Request focus

**Performance:**
- `SetVSync(bool)` - Enable/disable VSync
- `SetFramerateLimit(fps)` - Limit frame rate

**Backend:**
- DiaSFML (SFML sf::RenderWindow wrapper)

**Thread Safety:**
- ❌ Lifecycle/rendering methods Main thread only
- ✅ Query methods (GetWidth, IsOpen) thread-safe

**Best Practices:**
- Clear before rendering
- Display after rendering
- Check IsOpen before use
- Handle focus loss

**Gotchas:**
- SetFullscreen may recreate window
- Display blocks for VSync
- Don't mix VSync and frame rate limit

**[→ API Overview](../api-overview.md)**  
**[→ DiaSFML API](sfml-api.md)**  
**[→ DiaGraphics API](graphics-api.md)**
