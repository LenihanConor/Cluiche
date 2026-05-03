# Feature Spec: debug-console

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Adds `DiaVisualDebuggerConsole` — an in-game ImGui overlay activated by a configurable keypress — and introduces `DiaImGui` as a thin backend-abstraction module that decouples ImGui lifecycle management from any specific renderer. `DiaSFML` implements the SFML backend. `DiaVisualDebuggerConsole` depends on `DiaImGui` (for header access and lifecycle contract), not on `DiaSFML` — so when SFML is replaced, the console is unchanged.

The console provides: a checkbox tree to toggle `DebugLayerManager` layers, a metrics bar showing frame primitive count and dropped count, a DiaAPI command input field, and a log tail showing `DiaLogger` output. No new DiaInput dependency on the console itself — the caller invokes `Toggle()`.

**Problem solved:** During in-game development, toggling debug layers requires code changes or DiaAPI commands typed via an external tool. The console provides a keypress-accessible in-game surface, using the same DiaAPI command path, with zero overhead when hidden. The `DiaImGui` abstraction means the console survives a renderer swap.

---

## Acceptance Criteria

1. New `DiaImGui.vcxproj` static library in `Dia/DiaImGui/` with `IImGuiBackend` interface and `DiaImGuiManager`
2. `DiaSFML` implements `SFMLImGuiBackend : public IImGuiBackend`; imgui source files compiled into `DiaSFML.vcxproj`
3. `DiaSFML::RenderWindow` creates a `SFMLImGuiBackend`, registers it with `DiaImGuiManager`, and calls `DiaImGui::NewFrame()` / `DiaImGui::Render()` at the correct points in the render loop
4. `SFMLImGuiBackend::ProcessEvent(const sf::Event&)` is called directly (typed — not through the interface) in the SFML event loop; no `void*` event casting
5. `DiaVisualDebuggerConsole` class in a new `DiaVisualDebuggerConsole.vcxproj`; depends on `DiaImGui.vcxproj` (not `DiaSFML.vcxproj`)
6. `DiaVisualDebuggerConsole::Toggle()` — shows/hides the console window; starts hidden
7. `DiaVisualDebuggerConsole::Render(DebugLayerManager&, const DebugFrameData&)` — draws the ImGui window using `ImGui::*` calls directly; no-op when hidden
8. Layer checkbox tree shows all registered layers sorted by priority; checking/unchecking calls `EnableLayer()`/`DisableLayer()` on the passed `DebugLayerManager`
9. Metrics bar shows: `Primitives: N / 1024` and, when `DroppedCount() > 0`, a red `DROPPED: N` badge
10. Command input field: single-line `ImGui::InputText`; on Enter, dispatches string via `Dia::API::CommandRegistry::Execute()`
11. Log tail: shows last 64 `DiaLogger` warning/error lines; auto-scrolls to bottom; `kLogTailCapacity = 64`
12. `DiaVisualDebuggerConsole` subscribes to `DiaLogger` output via `Attach(DiaLogger&)`; `Detach()` removes it
13. `DiaVisualDebuggerConsole` has no DiaInput dependency — caller polls keypress and calls `Toggle()`
14. Build with no warnings; all tests pass

---

## DiaImGui — Backend Abstraction

### Purpose

`DiaImGui` owns the ImGui context lifecycle (`Init`, `Shutdown`, `NewFrame`, `Render`) behind an `IImGuiBackend` interface. Any renderer (SFML today, SDL/Vulkan/DX12 tomorrow) provides a concrete backend. `DiaVisualDebuggerConsole` and any other ImGui consumer depend only on `DiaImGui` — they call `ImGui::*` widget functions directly (those are stable and not wrapped).

### IImGuiBackend

```cpp
// Dia/DiaImGui/IImGuiBackend.h
namespace Dia::ImGui {

class IImGuiBackend
{
public:
    virtual ~IImGuiBackend() = default;
    virtual void Init()             = 0;
    virtual void Shutdown()         = 0;
    virtual void NewFrame(float dt) = 0;
    virtual void Render()           = 0;
};

} // namespace Dia::ImGui
```

`ProcessEvent` is deliberately **not** on the interface — event types are renderer-specific (`sf::Event`, `SDL_Event`, etc.). Each backend exposes a typed `ProcessEvent()` called directly by its renderer. No `void*` casting.

### DiaImGuiManager

```cpp
// Dia/DiaImGui/DiaImGuiManager.h
namespace Dia::ImGui {

class DiaImGuiManager
{
public:
    void SetBackend(IImGuiBackend* backend);  // call once at renderer startup
    IImGuiBackend* GetBackend() const;

    void Init();
    void Shutdown();
    void NewFrame(float dt);
    void Render();

private:
    IImGuiBackend* mBackend = nullptr;
};

// Convenience free functions — forward to a global DiaImGuiManager instance
void Init();
void Shutdown();
void NewFrame(float dt);
void Render();

} // namespace Dia::ImGui
```

`DiaVisualDebuggerConsole::Render()` is called after `DiaImGui::NewFrame()` has run. The manager asserts (or no-ops gracefully) if `mBackend == nullptr`.

---

## SFMLImGuiBackend (in DiaSFML)

```cpp
// Dia/DiaSFML/SFMLImGuiBackend.h
#include <DiaImGui/IImGuiBackend.h>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

namespace Dia::SFML {

class SFMLImGuiBackend : public Dia::ImGui::IImGuiBackend
{
public:
    explicit SFMLImGuiBackend(sf::RenderWindow& window);

    void Init()              override;  // ImGui::SFML::Init(mWindow)
    void Shutdown()          override;  // ImGui::SFML::Shutdown()
    void NewFrame(float dt)  override;  // ImGui::SFML::Update(mWindow, sf::seconds(dt))
    void Render()            override;  // ImGui::SFML::Render(mWindow)

    // Typed — called directly from RenderWindow event loop, not via IImGuiBackend
    void ProcessEvent(const sf::Event& event);

private:
    sf::RenderWindow& mWindow;
};

} // namespace Dia::SFML
```

### DiaSFML render loop changes

```cpp
// RenderWindow.h — new member
SFMLImGuiBackend mImGuiBackend;  // constructed with mWindowContext

// RenderWindow.cpp — constructor / Init()
mImGuiBackend.Init();
Dia::ImGui::SetBackend(&mImGuiBackend);  // register with global manager

// RenderWindow.cpp — event loop (existing pollEvent loop)
sf::Event event;
while (mWindowContext->pollEvent(event))
{
    mImGuiBackend.ProcessEvent(event);   // typed — direct call, not via interface
    // ... existing event handling ...
}

// RenderWindow.cpp — StartFrame()
Dia::ImGui::NewFrame(deltaTime);   // calls SFMLImGuiBackend::NewFrame → ImGui::SFML::Update

// RenderWindow.cpp — EndFrame() — after compositing, before display()
Dia::ImGui::Render();              // calls SFMLImGuiBackend::Render → ImGui::SFML::Render
mWindowContext->display();

// RenderWindow.cpp — destructor / Shutdown()
mImGuiBackend.Shutdown();
```

---

## DiaVisualDebuggerConsole class

```cpp
// Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h
namespace Dia::Debug {

class DiaVisualDebuggerConsole
{
public:
    static constexpr int kLogTailCapacity = 64;

    DiaVisualDebuggerConsole();
    ~DiaVisualDebuggerConsole();

    void Attach(Dia::Core::Logger& logger);
    void Detach();

    void Toggle();
    bool IsVisible() const;

    // Call each frame from the render thread, after DiaImGui::NewFrame() has run.
    void Render(DebugLayerManager& manager,
                const Dia::Graphics::DebugFrameData& debugFrameData);

private:
    void RenderLayerTree(DebugLayerManager& manager);
    void RenderMetricsBar(const Dia::Graphics::DebugFrameData& debugFrameData);
    void RenderCommandInput();
    void RenderLogTail();

    bool mVisible = false;

    char mLogBuffer[kLogTailCapacity][128] = {};
    int  mLogHead          = 0;
    int  mLogCount         = 0;
    bool mScrollToBottom   = false;

    char mCommandBuffer[256] = {};

    void* mLogSinkHandle = nullptr;  // opaque; set by Attach()
};

} // namespace Dia::Debug
```

The log buffer uses a plain `char[64][128]` 2D array — avoids `DynamicArrayC<char[128], N>` array-of-arrays which may not be supported.

---

## Render sections

### Layer tree

```cpp
void DiaVisualDebuggerConsole::RenderLayerTree(DebugLayerManager& manager)
{
    ImGui::Text("Debug Layers");
    ImGui::Separator();
    for (int i = 0; i < manager.GetLayerCount(); ++i)
    {
        Dia::Core::StringCRC name = manager.GetLayerName(i);
        bool enabled = manager.IsLayerEnabled(name);
        if (ImGui::Checkbox(name.AsString(), &enabled))
        {
            if (enabled) manager.EnableLayer(name);
            else         manager.DisableLayer(name);
        }
    }
}
```

Requires two new read-only accessors on `DebugLayerManager`:
- `int GetLayerCount() const`
- `Dia::Core::StringCRC GetLayerName(int index) const`

### Metrics bar

```cpp
void DiaVisualDebuggerConsole::RenderMetricsBar(const Dia::Graphics::DebugFrameData& debugFrameData)
{
    ImGui::Text("Primitives: %u / %u",
        debugFrameData.GetDebugPrimitiveCount(),
        Dia::Graphics::DebugFrameData::kCapacity);

    if (debugFrameData.DroppedCount() > 0)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1,0,0,1), "DROPPED: %u", debugFrameData.DroppedCount());
    }
}
```

### Command input

```cpp
void DiaVisualDebuggerConsole::RenderCommandInput()
{
    ImGui::Text("Command:");
    ImGui::SameLine();
    bool execute = ImGui::InputText("##cmd", mCommandBuffer, sizeof(mCommandBuffer),
                                    ImGuiInputTextFlags_EnterReturnsTrue);
    if (execute && mCommandBuffer[0] != '\0')
    {
        Dia::API::CommandRegistry::GetInstance().Execute(mCommandBuffer);
        mCommandBuffer[0] = '\0';
    }
}
```

### Log tail

```cpp
void DiaVisualDebuggerConsole::RenderLogTail()
{
    ImGui::BeginChild("LogTail", ImVec2(0, 120), false);
    for (int i = 0; i < mLogCount; ++i)
    {
        int idx = (mLogHead - mLogCount + i + kLogTailCapacity) % kLogTailCapacity;
        ImGui::TextUnformatted(mLogBuffer[idx]);
    }
    if (mScrollToBottom) { ImGui::SetScrollHereY(1.0f); mScrollToBottom = false; }
    ImGui::EndChild();
}
```

---

## DebugLayerManager accessor additions

```cpp
// DebugLayerManager.h — public section
int                  GetLayerCount() const;
Dia::Core::StringCRC GetLayerName(int index) const;
```

---

## Dependency graph

```
DiaVisualDebuggerConsole  →  DiaImGui (ImGui headers + lifecycle contract)
                 →  DiaVisualDebugger (DebugLayerManager)
                 →  DiaGraphics (DebugFrameData)
                 →  DiaAPI (CommandRegistry)
                 →  DiaCore (Logger)

DiaImGui         →  External/imgui/ (headers only; .cpp compiled in consumer)
                    no renderer dependency

DiaSFML          →  DiaImGui (IImGuiBackend)
                    compiles imgui .cpp + imgui-SFML.cpp
                    owns SFMLImGuiBackend

Game code        →  creates SFMLImGuiBackend (via DiaSFML RenderWindow)
                    creates DiaVisualDebuggerConsole
                    no direct DiaImGui calls needed
```

**Future renderer swap:** delete `SFMLImGuiBackend`, write `SDLImGuiBackend` in the new renderer module, call `Dia::ImGui::SetBackend(&sdlBackend)`. `DiaVisualDebuggerConsole` is unchanged.

---

## Registration Example (game code)

```cpp
// DiaSFML::RenderWindow::Init() handles backend registration automatically.

// At application startup:
static DiaVisualDebuggerConsole console;
console.Attach(DiaLogger::GetInstance());

// Each frame — game code owns keypress detection:
if (keyboard.WasKeyPressed(Key::kBacktick))
    console.Toggle();

// In render phase, after DiaImGui::NewFrame() has run (DiaSFML calls this):
console.Render(debugLayerManager, frameData.GetDebugFrameData());
```

---

## Files Changed

| File | Change |
|------|--------|
| `External/imgui/` | New — imgui source + headers (`imgui.cpp`, `imgui_draw.cpp`, `imgui_widgets.cpp`, `imgui_tables.cpp`, `imgui.h`, `imgui_internal.h`) |
| `Dia/DiaImGui/IImGuiBackend.h` | New |
| `Dia/DiaImGui/DiaImGuiManager.h` | New |
| `Dia/DiaImGui/DiaImGuiManager.cpp` | New |
| `Dia/DiaImGui/DiaImGui.vcxproj` | New project — references DiaCore; adds `External/imgui/` to include path |
| `Dia/DiaImGui/DiaImGui.vcxproj.filters` | New filters |
| `Dia/DiaSFML/SFMLImGuiBackend.h` | New |
| `Dia/DiaSFML/SFMLImGuiBackend.cpp` | New |
| `Dia/DiaSFML/imgui-SFML.h` | New — imgui-sfml bridge header |
| `Dia/DiaSFML/imgui-SFML.cpp` | New — imgui-sfml bridge implementation |
| `Dia/DiaSFML/RenderWindow.h` | Add `SFMLImGuiBackend mImGuiBackend`; remove `sf::Clock mImGuiClock` (owned by backend) |
| `Dia/DiaSFML/RenderWindow.cpp` | Replace `ImGui::SFML::*` direct calls with `DiaImGui::*` free functions; add `mImGuiBackend.ProcessEvent()` in event loop |
| `Dia/DiaSFML/DiaSFML.vcxproj` | Add `SFMLImGuiBackend.cpp`, imgui `.cpp` files, `imgui-SFML.cpp`; add `External/imgui/` to include paths; add `DiaImGui` project reference |
| `Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h` | New |
| `Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.cpp` | New |
| `Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.vcxproj` | New project — references `DiaImGui`, `DiaVisualDebugger`, `DiaGraphics`, `DiaAPI`, `DiaCore` |
| `Dia/DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.vcxproj.filters` | New filters |
| `Dia/DiaVisualDebugger/DebugLayerManager.h` | Add `GetLayerCount()`, `GetLayerName(int)` declarations |
| `Dia/DiaVisualDebugger/DebugLayerManager.cpp` | Implement new accessors; cache `mLastDroppedCount` in `Draw()` |
| `Cluiche/Cluiche.sln` | Add `DiaImGui.vcxproj` and `DiaVisualDebuggerConsole.vcxproj` |

**Prerequisites:** `debug-layer-manager` implemented; `debug-budget` implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add imgui source files to `External/imgui/` | imgui 1.90+ (MIT) |
| 2 | Create `DiaImGui.vcxproj` — write `IImGuiBackend.h`, `DiaImGuiManager.h/.cpp` | Thin module; no renderer deps |
| 3 | Add `DiaImGui.vcxproj` to `Cluiche.sln` | |
| 4 | Write `SFMLImGuiBackend.h/.cpp` in `Dia/DiaSFML/` | Wraps imgui-sfml; typed `ProcessEvent` |
| 5 | Update `DiaSFML.vcxproj` — add imgui `.cpp`, `imgui-SFML.cpp`, `SFMLImGuiBackend.cpp`; add `DiaImGui` project reference | |
| 6 | Modify `RenderWindow.h/.cpp` — replace direct `ImGui::SFML::*` calls with `DiaImGui::*`; add `mImGuiBackend.ProcessEvent()` in event loop | |
| 7 | Add `GetLayerCount()` / `GetLayerName(int)` to `DebugLayerManager` | |
| 8 | Create `DiaVisualDebuggerConsole.vcxproj` and write `DiaVisualDebuggerConsole.h/.cpp` | No DiaSFML dependency |
| 9 | Add `DiaVisualDebuggerConsole.vcxproj` to `Cluiche.sln` | |
| 10 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 11 | Write tests | `TestDiaVisualDebuggerConsole.cpp` |
| 12 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaVisualDebugger/TestDiaVisualDebuggerConsole.cpp`

Tests cover C++ logic only — no ImGui render output verification.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| Visibility | `DefaultHidden` | Fresh console → `IsVisible() == false` |
| Visibility | `Toggle_ShowsConsole` | `Toggle()` once → `IsVisible() == true` |
| Visibility | `Toggle_HidesConsole` | `Toggle()` twice → `IsVisible() == false` |
| LogTail | `Attach_LogMessage_AppearsInTail` | `Attach(logger)` + log warning → tail count == 1 |
| LogTail | `LogTail_CapacityLimit_OldestDropped` | Log `kLogTailCapacity + 5` messages → count == `kLogTailCapacity` |
| LogTail | `Detach_StopsReceivingLogs` | `Detach()` + log message → tail count unchanged |
| DiaImGuiManager | `SetBackend_GetBackend_RoundTrip` | Set backend → `GetBackend()` returns it |
| DiaImGuiManager | `NewFrame_NullBackend_NoAssert` | No backend set → `NewFrame()` no-ops gracefully |
| DebugLayerManager accessors | `GetLayerCount_AfterRegister_IsOne` | Register one layer → `GetLayerCount() == 1` |
| DebugLayerManager accessors | `GetLayerName_ReturnsRegisteredName` | `GetLayerName(0)` == registered layer's name |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — `GetLayerName()` returns `StringCRC`; `AsString()` used for ImGui display |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — `Render()` called from render phase |
| PD-003 | Component-based entities | Compliant — no entity concerns |
| PD-004 | No STL in public APIs | Compliant — `DiaVisualDebuggerConsole` and `DiaImGui` public APIs use `const char*`, `StringCRC`, plain types only |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — new `DiaImGui.vcxproj`, `DiaVisualDebuggerConsole.vcxproj`; `DiaSFML.vcxproj` updated |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant |
| PD-009 | Generated output under `Cluiche/out/` | Compliant |
| AD-001 | Module YAML frontmatter | Compliant — `dia.imgui.architecture.module.md` and `dia.debug.console.architecture.module.md` created |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — `Dia::ImGui::` for DiaImGui; `Dia::Debug::DiaVisualDebuggerConsole`; `Dia::SFML::SFMLImGuiBackend` |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — `DiaVisualDebuggerConsole.vcxproj` not linked in Release; `DiaImGui::NewFrame/Render` calls guarded in render loop |
| SD-DBG-011 | ImGui (MIT) + imgui-sfml approved | Compliant — this feature implements the approved integration via `DiaImGui` abstraction |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `DiaImGui` namespace clash | `Dia::ImGui` as a namespace could clash with ImGui's own global `ImGui::` namespace. Is this a problem? | No — `Dia::ImGui::` and `ImGui::` are distinct namespaces. Code calling `ImGui::Text()` is unambiguous. Code calling `Dia::ImGui::NewFrame()` is equally unambiguous. No collision. |
| 2 | imgui `.cpp` files compiled in DiaSFML | The imgui core `.cpp` files are compiled into `DiaSFML.vcxproj`. If a second renderer (SDL) is added, it also needs to compile them. Will there be duplicate symbol conflicts? | Each renderer vcxproj compiles imgui into its own static lib — no linking conflict as long as only one renderer links at a time. If both link simultaneously (edge case), move imgui `.cpp` files into `DiaImGui.vcxproj` instead. For now, one renderer at a time is assumed; keep them in `DiaSFML.vcxproj`. |
| 3 | `DiaImGuiManager` global singleton | The free functions (`Dia::ImGui::NewFrame()` etc.) forward to a global manager instance. Is a global appropriate here? | Yes — there is exactly one ImGui context per process. A global is the correct model and matches how ImGui itself works. No need for DI here. |
| 4 | `CommandRegistry::Execute()` signature | Does `CommandRegistry` have an `Execute(const char*)` method? | Must be verified at Task 8. If not, tokenise the string manually and call the callback directly. |
| 5 | DiaLogger sink API | What is the DiaLogger sink interface? | Must be checked at Task 8. Implement as a `LogObserver` if the observer pattern is used; otherwise add a minimal sink API to DiaLogger. |
| 6 | imgui version pinning | Which exact imgui version? | Pin to imgui 1.90+ and imgui-sfml 2.6+ — last versions supporting SFML 2.x. When SFML is replaced, upgrade both together. |

---

## Status

`Approved`
