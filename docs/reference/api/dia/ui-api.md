# DiaUI API

**Last Updated:** 2026-04-01

User interface abstraction API providing platform-agnostic UI integration.

---

## Overview

**DiaUI** provides a platform-agnostic interface for integrating UI systems into the application.

**Location:** `Dia/DiaUI/`

**Namespace:** `Dia::UI::`

**Key Concepts:**
- **IUISystem** - Abstract UI interface
- **Platform-agnostic** - Backend implementation (Awesomium, future: CEF/ImGui)
- **Thread ownership** - UI typically runs on Main thread

**Implementation:** `Dia::UIAwesomium::UISystem` (Awesomium backend - deprecated)

---

## Core Interface

### IUISystem

**Header:** `Dia/DiaUI/Interface/IUISystem.h`

**Purpose:** Abstract UI system interface implemented by backend

#### Key Methods

```cpp
class IUISystem
{
public:
    virtual ~IUISystem() = default;
    
    // Lifecycle
    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Shutdown() = 0;
    
    // UI creation
    virtual void LoadUI(const char* url) = 0;
    virtual void UnloadUI() = 0;
    
    // UI interaction
    virtual void ExecuteJavaScript(const char* script) = 0;
    virtual void BindCallback(const char* name, void (*callback)(void)) = 0;
    
    // Query
    virtual bool IsLoaded() const = 0;
    virtual bool IsVisible() const = 0;
    
    // Visibility
    virtual void Show() = 0;
    virtual void Hide() = 0;
};
```

#### Usage Example

```cpp
// Get UI system (usually from application)
Dia::UI::IUISystem* uiSystem = GetUISystem();

// Initialize
uiSystem->Initialize();

// Load UI from URL
uiSystem->LoadUI("file:///ui/mainmenu.html");

// Execute JavaScript
uiSystem->ExecuteJavaScript("document.getElementById('title').textContent = 'Game Title';");

// Bind C++ callback to JavaScript
uiSystem->BindCallback("OnPlayClicked", &OnPlayButtonClicked);

// Update each frame (call from Main thread)
uiSystem->Update();

// Show/hide
uiSystem->Show();
uiSystem->Hide();

// Cleanup
uiSystem->UnloadUI();
uiSystem->Shutdown();
```

---

## Common Patterns

### UI Lifecycle

```cpp
class UIModule : public Dia::Application::Module
{
public:
    void OnConstruct() override
    {
        mUISystem = CreateUISystem();  // Backend-specific
    }
    
    void OnStart() override
    {
        mUISystem->Initialize();
        mUISystem->LoadUI("file:///ui/game.html");
        mUISystem->Show();
    }
    
    void OnUpdate() override
    {
        mUISystem->Update();  // Process UI events
    }
    
    void OnStop() override
    {
        mUISystem->Hide();
        mUISystem->UnloadUI();
        mUISystem->Shutdown();
    }
    
    void OnDestruct() override
    {
        delete mUISystem;
        mUISystem = nullptr;
    }
    
private:
    Dia::UI::IUISystem* mUISystem;
};
```

---

### JavaScript ↔ C++ Binding

```cpp
// C++ side
void OnButtonClicked()
{
    StartGame();
}

void SetupUI(Dia::UI::IUISystem* ui)
{
    // Bind C++ function to JavaScript
    ui->BindCallback("onPlayButtonClicked", &OnButtonClicked);
    
    // Execute JavaScript to setup event
    ui->ExecuteJavaScript(R"(
        document.getElementById('playButton').addEventListener('click', function() {
            onPlayButtonClicked();  // Calls C++ function
        });
    )");
}
```

```javascript
// JavaScript side (in HTML/JS file)
function updateScore(score) {
    document.getElementById('score').textContent = score;
}

// Called from C++
```

```cpp
// C++ calling JavaScript
void UpdateScore(Dia::UI::IUISystem* ui, int score)
{
    char script[128];
    sprintf(script, "updateScore(%d);", score);
    ui->ExecuteJavaScript(script);
}
```

---

### Proxy Pattern for Thread Safety

```cpp
// Sim thread → Main thread UI communication
class UIProxy
{
public:
    UIProxy(Dia::UI::IUISystem* uiSystem)
        : mUISystem(uiSystem)
    {}
    
    // Called from Sim thread
    void SetScore(int score)
    {
        mPendingCommands.Add([this, score]() {
            char script[128];
            sprintf(script, "updateScore(%d);", score);
            mUISystem->ExecuteJavaScript(script);
        });
    }
    
    // Called from Main thread
    void FlushCommands()
    {
        for (auto& command : mPendingCommands)
        {
            command();
        }
        mPendingCommands.Clear();
    }
    
private:
    Dia::UI::IUISystem* mUISystem;
    Dia::Core::DynamicArray<std::function<void()>> mPendingCommands;
};
```

---

### State Synchronization

```cpp
class GameUI
{
public:
    void SyncState(Dia::UI::IUISystem* ui, const GameState& state)
    {
        // Update all UI elements
        UpdateScore(ui, state.score);
        UpdateHealth(ui, state.health);
        UpdateAmmo(ui, state.ammo);
    }
    
private:
    void UpdateScore(Dia::UI::IUISystem* ui, int score)
    {
        char script[128];
        sprintf(script, "document.getElementById('score').textContent = %d;", score);
        ui->ExecuteJavaScript(script);
    }
    
    void UpdateHealth(Dia::UI::IUISystem* ui, float health)
    {
        char script[128];
        sprintf(script, "document.getElementById('health').style.width = '%.0f%%';", health);
        ui->ExecuteJavaScript(script);
    }
    
    void UpdateAmmo(Dia::UI::IUISystem* ui, int ammo)
    {
        char script[128];
        sprintf(script, "document.getElementById('ammo').textContent = %d;", ammo);
        ui->ExecuteJavaScript(script);
    }
};
```

---

## Backend Implementation

### Awesomium Implementation (Deprecated)

**Class:** `Dia::UIAwesomium::UISystem`

**Header:** `Dia/DiaUIAwesomium/UISystem.h`

**Status:** ⚠️ **DEPRECATED** - Awesomium is no longer maintained

```cpp
class UISystem : public Dia::UI::IUISystem
{
public:
    UISystem(int width, int height);
    
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
};
```

**[→ UIAwesomium API Details](ui-awesomium-api.md)**

---

### Future Backend Options

**CEF (Chromium Embedded Framework):**
- Modern replacement for Awesomium
- Actively maintained
- Better performance
- Full Chrome rendering

**ImGui:**
- Immediate-mode GUI
- Game-focused
- No HTML/CSS/JS (pure C++)
- Lower overhead
- Better for debug UI and tools

**[→ Future Directions](../../02-design/future-directions.md)**

---

## Dependencies

**Required:**
- None (self-contained interface)

**Implementation:**
- `Dia/DiaUIAwesomium/` - Awesomium backend (deprecated)

**Future:**
- CEF backend (planned)
- ImGui backend (planned)

---

## Thread Safety

| Method | Thread Safety |
|--------|---------------|
| `Initialize/Shutdown` | ❌ Call from Main thread only |
| `Update()` | ❌ Call from Main thread only |
| `LoadUI/UnloadUI` | ❌ Call from Main thread only |
| `ExecuteJavaScript` | ❌ Call from Main thread only |
| `BindCallback` | ❌ Call from Main thread only |

**Important:** All IUISystem methods must be called from the **Main thread**. Use proxy pattern for cross-thread communication.

---

## Best Practices

### 1. Always Update Each Frame

```cpp
// ✅ Good: Update UI every frame
void MainLoop()
{
    uiSystem->Update();  // Process UI events
    Render();
}

// ❌ Bad: No update (UI freezes)
void MainLoop()
{
    Render();
}
```

---

### 2. Use Proxy for Cross-Thread Access

```cpp
// ✅ Good: Proxy pattern
// Sim thread
uiProxy->SetScore(100);

// Main thread
uiProxy->FlushCommands();  // Executes queued commands

// ❌ Bad: Direct cross-thread access
// Sim thread
uiSystem->ExecuteJavaScript("...");  // NOT THREAD-SAFE!
```

---

### 3. Sanitize JavaScript Strings

```cpp
// ✅ Good: Escape quotes
void SetText(const char* text)
{
    std::string escaped = EscapeJavaScript(text);
    char script[256];
    sprintf(script, "setText('%s');", escaped.c_str());
    uiSystem->ExecuteJavaScript(script);
}

// ❌ Bad: No escaping (injection vulnerability)
void SetText(const char* text)
{
    char script[256];
    sprintf(script, "setText('%s');", text);  // If text contains '
    uiSystem->ExecuteJavaScript(script);
}
```

---

### 4. Check IsLoaded Before Interaction

```cpp
// ✅ Good: Check before use
if (uiSystem->IsLoaded())
{
    uiSystem->ExecuteJavaScript("updateScore(100);");
}

// ❌ Bad: No check (crash if not loaded)
uiSystem->ExecuteJavaScript("updateScore(100);");
```

---

## Gotchas

### Gotcha 1: Callbacks Are Synchronous

Callbacks bound with `BindCallback()` execute **immediately** in the UI thread. Don't block:

```cpp
// ✅ Good: Queue action
void OnButtonClicked()
{
    QueueAction(StartGame);  // Non-blocking
}

// ❌ Bad: Block UI thread
void OnButtonClicked()
{
    StartGame();  // Blocks for seconds
}
```

---

### Gotcha 2: JavaScript Context Lifetime

JavaScript state is **lost** when UI is unloaded:

```cpp
// Set some JavaScript state
uiSystem->ExecuteJavaScript("var score = 100;");

// Unload UI
uiSystem->UnloadUI();

// Reload UI
uiSystem->LoadUI("file:///ui/game.html");

// score is undefined (state lost)
uiSystem->ExecuteJavaScript("console.log(score);");  // undefined
```

---

### Gotcha 3: File:// Protocol

UI files loaded via `file://` protocol have **limited permissions**:

```cpp
// ✅ Good: file:// with absolute path
uiSystem->LoadUI("file:///C:/Game/ui/index.html");

// ⚠️ Limited: file:// can't make HTTP requests
// JavaScript fetch() may not work

// ✅ Alternative: Use embedded web server
uiSystem->LoadUI("http://localhost:8080/ui/index.html");
```

---

## Limitations

### Current Limitations

1. **Single UI view** - Only one UI can be loaded at a time
2. **No multi-window support** - Can't have multiple independent UI windows
3. **Backend deprecated** - Awesomium no longer maintained
4. **No GPU acceleration** - Software rendering only (Awesomium limitation)
5. **No WebGL** - Limited to HTML5 Canvas 2D (Awesomium limitation)

### Future Improvements

- **CEF backend** - Modern Chromium-based UI (hardware accelerated, WebGL)
- **ImGui backend** - Lightweight immediate-mode GUI for debug/tools
- **Multi-view support** - Multiple independent UI panels
- **Better thread safety** - Built-in proxy/command queue

**[→ Future Directions](../../02-design/future-directions.md)**

---

## Migration Path

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
ui->LoadUI("http://localhost:8080/ui/game.html");
```

**Key Differences:**
- CEF requires HTTP server (not file://)
- Better performance (GPU accelerated)
- Modern web standards (ES6, WebGL)
- Ongoing maintenance

---

### From Web UI to ImGui

**Current (Web):**
```html
<div id="score">Score: 0</div>
```

```cpp
uiSystem->ExecuteJavaScript("document.getElementById('score').textContent = 'Score: 100';");
```

**Future (ImGui):**
```cpp
ImGui::Begin("HUD");
ImGui::Text("Score: %d", score);
ImGui::End();
```

**Key Differences:**
- No HTML/CSS/JS (pure C++)
- Immediate-mode (rebuild each frame)
- Better for debug UI and tools
- Lower overhead

---

## Examples

### Full UI Module

```cpp
class MainUIModule : public Dia::Application::Module
{
public:
    MainUIModule()
        : mUISystem(nullptr)
    {}
    
    void OnConstruct() override
    {
        mUISystem = new Dia::UIAwesomium::UISystem(1920, 1080);
    }
    
    void OnStart() override
    {
        mUISystem->Initialize();
        mUISystem->LoadUI("file:///ui/mainmenu.html");
        
        // Bind callbacks
        mUISystem->BindCallback("onPlayClicked", &OnPlayClicked);
        mUISystem->BindCallback("onQuitClicked", &OnQuitClicked);
        
        mUISystem->Show();
    }
    
    void OnUpdate() override
    {
        mUISystem->Update();
    }
    
    void OnStop() override
    {
        mUISystem->Hide();
        mUISystem->UnloadUI();
        mUISystem->Shutdown();
    }
    
    void OnDestruct() override
    {
        delete mUISystem;
        mUISystem = nullptr;
    }
    
private:
    static void OnPlayClicked()
    {
        // Queue transition to game level
        QueueLevelTransition("GameLevel");
    }
    
    static void OnQuitClicked()
    {
        // Queue application exit
        QueueApplicationExit();
    }
    
    Dia::UI::IUISystem* mUISystem;
};
```

---

### HUD Update Pattern

```cpp
class HUDModule : public Dia::Application::Module
{
public:
    void OnStart() override
    {
        mUISystem->LoadUI("file:///ui/hud.html");
        mUISystem->Show();
        
        // Subscribe to game state changes
        mObserver.Observe(&GameState::GetSubject(), [this](const GameState& state) {
            UpdateHUD(state);
        });
    }
    
    void UpdateHUD(const GameState& state)
    {
        // Update all HUD elements
        char script[512];
        sprintf(script, R"(
            document.getElementById('health').style.width = '%.0f%%';
            document.getElementById('score').textContent = %d;
            document.getElementById('ammo').textContent = '%d/%d';
        )", state.health, state.score, state.ammo, state.maxAmmo);
        
        mUISystem->ExecuteJavaScript(script);
    }
    
private:
    Dia::UI::IUISystem* mUISystem;
    Dia::Core::Observer mObserver;
};
```

---

## Summary

**Core Interface:**
- `IUISystem` - Abstract UI system

**Lifecycle:**
- `Initialize()` - Setup UI backend
- `Update()` - Process UI events (call each frame)
- `Shutdown()` - Cleanup

**UI Loading:**
- `LoadUI(url)` - Load HTML/JS UI
- `UnloadUI()` - Unload current UI

**Interaction:**
- `ExecuteJavaScript(script)` - C++ → JavaScript
- `BindCallback(name, callback)` - JavaScript → C++

**Visibility:**
- `Show()/Hide()` - Control visibility

**Backend:**
- Awesomium (deprecated, software rendering)
- Future: CEF (hardware accelerated, modern web)
- Future: ImGui (immediate-mode, C++ only)

**Thread Safety:**
- ❌ All methods Main thread only
- ✅ Use proxy pattern for cross-thread communication

**Best Practices:**
- Update every frame
- Use proxy for cross-thread access
- Sanitize JavaScript strings
- Check IsLoaded before interaction

**Limitations:**
- Single UI view
- Deprecated backend
- No GPU acceleration (Awesomium)

**[→ API Overview](../api-overview.md)**  
**[→ UIAwesomium API](ui-awesomium-api.md)**  
**[→ DiaApplication API](application-api.md)**
