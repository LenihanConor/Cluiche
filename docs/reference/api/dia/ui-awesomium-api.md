# DiaUIAwesomium API

**Last Updated:** 2026-04-01

**Status:** ⚠️ **DEPRECATED** - Awesomium is no longer maintained

Awesomium-based UI backend (deprecated, for reference only).

---

## Overview

**DiaUIAwesomium** provides an Awesomium-based implementation of the UI system.

**Location:** `Dia/DiaUIAwesomium/`

**Namespace:** `Dia::UIAwesomium::`

**Status:** ⚠️ **DEPRECATED**

**Why Deprecated:**
- Awesomium SDK no longer maintained (last update: 2015)
- No support for modern web standards (stuck on Chromium 27)
- Software rendering only (no GPU acceleration)
- No WebGL support
- Security vulnerabilities unfixed

**Migration Path:** Migrate to CEF (Chromium Embedded Framework) or ImGui

---

## Core Class

### UISystem

**Header:** `Dia/DiaUIAwesomium/UISystem.h`

**Purpose:** Awesomium implementation of IUISystem

#### Key Methods

```cpp
class UISystem : public Dia::UI::IUISystem
{
public:
    UISystem(int width, int height);
    ~UISystem();
    
    // IUISystem implementation
    void Initialize() override;
    void Update() override;
    void Shutdown() override;
    
    void LoadUI(const char* url) override;
    void UnloadUI() override;
    
    void ExecuteJavaScript(const char* script) override;
    void BindCallback(const char* name, void (*callback)(void)) override;
    
    bool IsLoaded() const override;
    bool IsVisible() const override;
    
    void Show() override;
    void Hide() override;
    
private:
    Awesomium::WebCore* mWebCore;
    Awesomium::WebView* mWebView;
    int mWidth;
    int mHeight;
    bool mVisible;
};
```

#### Usage Example (For Reference Only)

```cpp
// ⚠️ DEPRECATED - Do not use in new code
using namespace Dia::UIAwesomium;

// Create UI system
UISystem* uiSystem = new UISystem(1920, 1080);

// Initialize
uiSystem->Initialize();

// Load UI
uiSystem->LoadUI("file:///ui/mainmenu.html");

// Execute JavaScript
uiSystem->ExecuteJavaScript("document.getElementById('title').textContent = 'Game';");

// Update each frame
uiSystem->Update();

// Cleanup
uiSystem->UnloadUI();
uiSystem->Shutdown();
delete uiSystem;
```

---

## Known Issues

### Performance Issues

1. **Software rendering only** - No GPU acceleration, poor performance
2. **Slow JavaScript execution** - Old V8 engine (Chromium 27)
3. **Memory leaks** - Known leaks in Awesomium core
4. **Update blocking** - Update() can block for milliseconds

### Compatibility Issues

1. **No modern web standards** - ES6, WebGL, modern CSS not supported
2. **Limited HTML5 support** - Canvas 2D only, no audio/video elements
3. **No WebSockets** - No real-time communication
4. **Outdated Chromium** - Based on Chromium 27 (2013)

### Security Issues

1. **Unpatched vulnerabilities** - No security updates since 2015
2. **XSS risks** - Limited sandboxing
3. **File access** - file:// protocol has broad permissions

---

## Limitations

**Technical Limitations:**
- Single view only (can't have multiple UI panels)
- No transparency (alpha channel not supported)
- No multi-window support
- Limited input handling (no gamepad, no touch)
- Fixed size (resize requires recreate)

**Web Standards Support:**
- ✅ HTML4, CSS2.1
- ⚠️ HTML5 (partial)
- ❌ ES6/ES7 JavaScript
- ❌ WebGL
- ❌ WebAudio
- ❌ WebRTC
- ❌ ServiceWorkers

---

## Migration Guide

### From Awesomium to CEF

**Current (Awesomium):**
```cpp
Dia::UIAwesomium::UISystem* ui = new Dia::UIAwesomium::UISystem(1920, 1080);
ui->Initialize();
ui->LoadUI("file:///ui/game.html");
```

**Future (CEF):**
```cpp
Dia::UICEF::UISystem* ui = new Dia::UICEF::UISystem(1920, 1080);
ui->Initialize();
ui->LoadUI("http://localhost:8080/ui/game.html");  // HTTP, not file://
```

**Key Differences:**
- CEF requires HTTP server (not file://)
- CEF supports modern web standards (ES6, WebGL)
- CEF has GPU acceleration
- CEF is actively maintained

---

### From Web UI to ImGui

**Current (Web UI):**
```html
<div id="hud">
    <div id="health">Health: 100</div>
    <div id="score">Score: 0</div>
</div>
```

```cpp
uiSystem->ExecuteJavaScript("document.getElementById('score').textContent = 'Score: 100';");
```

**Future (ImGui):**
```cpp
// Immediate-mode (rebuilt each frame)
ImGui::Begin("HUD");
ImGui::Text("Health: %d", health);
ImGui::Text("Score: %d", score);
ImGui::End();
```

**Key Differences:**
- ImGui is immediate-mode (rebuild each frame vs retain-mode)
- No HTML/CSS/JavaScript (pure C++)
- Better performance (lightweight)
- Better for debug UI and tools (not end-user facing)

---

## Replacement Options

### Option 1: CEF (Chromium Embedded Framework)

**Pros:**
- Modern Chromium (actively maintained)
- Full web standards support (ES6, WebGL, etc.)
- GPU acceleration
- Same HTML/CSS/JS as current UI

**Cons:**
- Large binary size (~200 MB)
- More complex integration
- Higher memory usage

**Best For:** Projects needing full web UI (complex menus, web-based tools)

**[→ CEF Documentation](https://bitbucket.org/chromiumembedded/cef)**

---

### Option 2: ImGui

**Pros:**
- Lightweight (~100 KB)
- Fast (immediate-mode)
- Easy integration
- Great debug tools

**Cons:**
- C++ only (no HTML/CSS/JS)
- Immediate-mode requires different thinking
- Less suitable for complex, styled UIs

**Best For:** Debug UI, tools, in-game developer console, simple HUDs

**[→ ImGui Documentation](https://github.com/ocornut/imgui)**

---

### Option 3: Noesis GUI

**Pros:**
- XAML-based (similar to WPF)
- Hardware accelerated
- Good performance
- Rich styling

**Cons:**
- Commercial license required
- XAML learning curve
- Heavier than ImGui

**Best For:** Professional game UI (AAA-style menus and HUDs)

**[→ Noesis GUI](https://www.noesisengine.com/)**

---

## Deprecation Timeline

**Current Status:** DEPRECATED (as of 2026-04-01)

**What This Means:**
- ⚠️ No new features
- ⚠️ No bug fixes
- ⚠️ Existing code still compiles (for now)
- ⚠️ Will be removed in future major version

**Recommended Actions:**
1. Audit current UI usage
2. Choose replacement (CEF or ImGui)
3. Plan migration timeline
4. Implement replacement backend
5. Migrate UI code incrementally
6. Remove Awesomium dependency

---

## Summary

**Class:**
- `UISystem` - Awesomium implementation of IUISystem

**Status:**
- ⚠️ **DEPRECATED** - Do not use in new code

**Issues:**
- Software rendering only
- Outdated web standards
- Security vulnerabilities
- No longer maintained

**Migration Options:**
- **CEF** - Full web UI, modern standards
- **ImGui** - Lightweight, C++ only, immediate-mode
- **Noesis GUI** - XAML-based, commercial

**Timeline:**
- Currently deprecated
- Plan removal in next major version
- Migrate to replacement ASAP

**Next Steps:**
1. Choose replacement (CEF recommended for web UI continuity)
2. Implement new backend
3. Migrate UI assets and code
4. Remove Awesomium dependency

**[→ API Overview](../api-overview.md)**  
**[→ DiaUI API](ui-api.md)**  
**[→ Future Directions](../../design-rationale/future-directions.md)**  
**[→ External Links](../../registry/external-links.md)**
