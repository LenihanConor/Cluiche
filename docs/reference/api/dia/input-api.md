# DiaInput API

**Last Updated:** 2026-11-20

Input abstraction API providing platform-agnostic input event handling.

---

## Overview

**DiaInput** provides a platform-agnostic input system for keyboard, mouse, joystick and console gamepad events.

**Location:** `Dia/DiaInput/`

**Namespace:** `Dia::Input::`

**Key Concepts:**
- **Event** - Tagged-union input event (`Dia/DiaInput/Event.h`)
- **EventData** - Fixed-capacity buffer of events for one frame
- **IInputSource** - Interface implemented by backends that poll a device API
- **InputSourceManager** - Polls all registered sources into one `EventData`
- **InputState** - Query API (down / pressed / released) built from events
- **ActionMap** - Binds named actions (`ActionID`, a `StringCRC`) to keys, buttons and axes
- **Platform-agnostic** - Backend implementation in DiaSDL

**Implementation:** `Dia::SDL::InputSource` (SDL3 backend; `Dia::SDL::Window` is also the input source)

---

## Core Classes

### Event

**Header:** `Dia/DiaInput/Event.h`

**Purpose:** One input event. `type` selects which union member is valid.

#### Event Types

```cpp
Event::EType:
    kClosed, kResized, kLostFocus, kGainedFocus, kTextEntered,
    kKeyPressed, kKeyReleased,
    kMouseWheelMoved, kMouseButtonPressed, kMouseButtonReleased,
    kMouseMoved, kMouseEntered, kMouseLeft,
    kJoystickButtonPressed, kJoystickButtonReleased, kJoystickMoved,
    kJoystickConnected, kJoystickDisconnected,
    kConsoleGamepadButtonPressed, kConsoleGamepadButtonReleased,
    kConsoleGamepadAnalogStickMove, kConsoleGamepadAnalogTriggers,
    kConsoleGamepadConnected, kConsoleGamepadDisconnected
```

Key codes live in `EKey` (`Dia/DiaInput/EKey.h`), mouse buttons in `EMouseButton` (`kLeft`, `kRight`, `kMiddle`), joystick axes in `EJoystickAxis`, and console gamepad buttons in `ConsoleGamepad::EButtonID`.

#### Event Structure

```cpp
class Event
{
public:
    EType type;

    union
    {
        SizeEvent            size;             // kResized: width, height
        KeyEvent             key;              // code, alt, control, shift, system; AsKey()
        TextEvent            text;             // unicode (UTF-32)
        MouseMoveEvent       mouseMove;        // x, y
        MouseButtonEvent     mouseButton;      // button, x, y; AsMouseButton()
        MouseWheelEvent      mouseWheel;       // int delta, x, y
        JoystickConnectEvent joystickConnect;  // joystickId
        JoystickMoveEvent    joystickMove;     // joystickId, axis, float position in [-100 .. 100]
        JoystickButtonEvent  joystickButton;   // joystickId, button
        ConsoleGamepadConnectEvent         consoleGamepadConnectEvent;
        ConsoleGamepadMoveEvent            consoleGamepadMoveEvent;
        ConsoleGamepadButtonEvent          consoleGamepadButtonEvent;
        ConsoleGamepadAnalogueTriggerEvent consoleGamepadAnalogueTriggerEvent;
    };
};
```

Joystick axis positions are normalised to the range -100 to 100 by the backend.

Always check `type` before reading a union member.

#### Usage Example

```cpp
void HandleEvent(const Dia::Input::Event& event)
{
    switch (event.type)
    {
        case Dia::Input::Event::EType::kKeyPressed:
        {
            Dia::Input::EKey key = event.key.AsKey();
            bool shift = event.key.shift;
            break;
        }
        case Dia::Input::Event::EType::kMouseMoved:
        {
            int x = event.mouseMove.x;
            int y = event.mouseMove.y;
            break;
        }
        case Dia::Input::Event::EType::kJoystickMoved:
        {
            float position = event.joystickMove.position;   // -100 .. 100
            break;
        }
        default:
            break;
    }
}
```

---

### IInputSource

**Header:** `Dia/DiaInput/IInputSource.h`

```cpp
class IInputSource
{
public:
    enum class Priority { Lowest = 0, Low = 25, Normal = 50, High = 75, Highest = 100 };

    IInputSource(Priority priority = Priority::Normal);

    virtual void StartFrame() {}
    virtual void Poll(EventData& outStream) = 0;   // append, never clear
    virtual void EndFrame() {}

    int GetUniqueID() const;
    Priority GetPriority() const;
    void SetPriority(Priority priority);

    virtual void ListenForInputSources(Dia::Core::BitArray8 sources) {}
};
```

---

### InputSourceManager

**Header:** `Dia/DiaInput/InputSourceManager.h`

**Purpose:** Manages multiple input sources

```cpp
class InputSourceManager
{
public:
    void AddInputSource(IInputSource* inputSource);      // does not take ownership
    void RemoveInputSource(IInputSource* inputSource);

    void StartFrame();
    void Update(EventData& outStream);                   // polls all sources, appends events
    void EndFrame();
};
```

#### Usage Example

```cpp
Dia::Input::InputSourceManager inputManager;
inputManager.AddInputSource(sdlWindow);   // Dia::SDL::Window* is also an IInputSource

Dia::Input::EventData events;

inputManager.StartFrame();
inputManager.Update(events);
inputManager.EndFrame();

for (unsigned int i = 0; i < events.Size(); ++i)
{
    HandleEvent(events[i]);
}
```

---

### InputState

**Header:** `Dia/DiaInput/InputState.h`

Built from events with `ProcessEvents(const EventData&)`; cleared per frame with `ClearFrameState()`.

```cpp
bool IsKeyDown(EKey key) const;
bool WasKeyPressed(EKey key) const;
bool WasKeyReleased(EKey key) const;

bool IsMouseButtonDown(EMouseButton button) const;
bool WasMouseButtonPressed(EMouseButton button) const;
bool WasMouseButtonReleased(EMouseButton button) const;
void GetMousePosition(int& x, int& y) const;
int  GetMouseWheelDelta() const;

bool IsGamepadConnected(unsigned int index) const;
bool IsGamepadButtonDown(unsigned int index, ConsoleGamepad::EButtonID button) const;

bool  IsJoystickConnected(unsigned int index) const;
bool  IsJoystickButtonDown(unsigned int index, unsigned int button) const;
float GetJoystickAxisValue(unsigned int index, EJoystickAxis axis) const;
```

---

### ActionMap

**Header:** `Dia/DiaInput/ActionMap.h`

```cpp
using ActionID = Core::StringCRC;

void BindKey(ActionID action, EKey key);
void BindMouseButton(ActionID action, EMouseButton button);
void BindGamepadButton(ActionID action, unsigned int gamepadIndex, ConsoleGamepad::EButtonID button);
void BindJoystickButton(ActionID action, unsigned int joystickIndex, unsigned int button);
void BindJoystickAxis(ActionID action, unsigned int joystickIndex, EJoystickAxis axis);
void UnbindAction(ActionID action);
void ClearAllBindings();

void ProcessEvents(const EventData& events);
bool  IsActionActive(ActionID action) const;
float GetActionValue(ActionID action) const;

bool SaveProfile(const char* filePath, const char* profileName = "Default") const;
bool LoadProfile(const char* filePath);
```

```cpp
Dia::Input::ActionMap actionMap;
actionMap.BindKey(Dia::Input::ActionID("Jump"), Dia::Input::EKey::Space);
actionMap.BindGamepadButton(Dia::Input::ActionID("Jump"), 0, Dia::Input::ConsoleGamepad::EButtonID::A);

actionMap.ProcessEvents(events);
if (actionMap.IsActionActive(Dia::Input::ActionID("Jump")))
{
    Jump();
}
```

---

## Backend Implementation

### DiaSDL Implementation

**Class:** `Dia::SDL::InputSource` (base of `Dia::SDL::Window`)

**Header:** `Dia/DiaSDL/InputSource.h`

```cpp
class InputSource : public Dia::Input::IInputSource
{
public:
    void SetWindowContext(SDL_Window* window);
    void ListenForInputSources(Dia::Core::BitArray8 listeningToSource) override;
    void Poll(Dia::Input::EventData& outStream) override;

    static Dia::Input::EKey SDLKeycodeToEKey(SDL_Keycode key);
    static bool TranslateSDLEvent(const SDL_Event& sdlEvent, Dia::Input::Event& outEvent);
};
```

`Poll` drains SDL events and converts them to `Dia::Input::Event`. Source categories for `ListenForInputSources` are `InputSource::ESourceIndex::kSystem`, `kKeyboard`, `kMouse` and `kJoystick`.

**[→ DiaSDL API Details](sdl-api.md)**

---

## Dependencies

**Required:**
- DiaCore

**Implementation:**
- `Dia/DiaSDL/` - SDL3 backend

---

## Thread Safety

Input sources are polled on the **main thread**, which owns the window. Forward events to the simulation thread through a stream rather than sharing state.

---

## Best Practices

- Poll on the main thread and forward events to the Sim thread.
- Use `InputState` for continuous input (held keys, axes) and events for discrete actions.
- Use `ActionMap` so game code binds to action names, not raw keys.
- Give UI input sources a higher `Priority` so they see events before game input.

---

## Gotchas

- `Poll` appends to the shared `EventData`; never clear it inside a source.
- `EventData` has a fixed capacity (64 events by default); the manager logs a warning when it fills.
- Check `Event::type` before reading any union member.

---

## Summary

**Event categories:**
- Window (close, resize, focus), keyboard, text, mouse, joystick, console gamepad

**Input Flow:**
1. Backend (DiaSDL) captures events
2. Converts to `Dia::Input::Event`
3. Main thread polls through `InputSourceManager::Update`
4. Forwards to Sim thread

**[→ API Overview](../api-overview.md)**  
**[→ DiaSDL API](sdl-api.md)**  
**[→ DiaApplicationFlow API](application-api.md)**
