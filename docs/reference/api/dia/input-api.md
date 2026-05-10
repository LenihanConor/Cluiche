# DiaInput API

**Last Updated:** 2026-04-01

Input abstraction API providing platform-agnostic input event handling.

---

## Overview

**DiaInput** provides a platform-agnostic input system for keyboard, mouse, and gamepad events.

**Location:** `Dia/DiaInput/`

**Namespace:** `Dia::Input::`

**Key Concepts:**
- **InputEvent** - Platform-agnostic input events
- **InputSourceManager** - Manages multiple input sources
- **Platform-agnostic** - Backend implementation in DiaSFML

**Implementation:** `Dia::SFML::DiaSFMLInputSource` (SFML backend)

---

## Core Classes

### InputEvent

**Header:** `Dia/DiaInput/DiaInputEvent.h`

**Purpose:** Platform-agnostic input event structure

#### Event Types

```cpp
enum class InputEventType
{
    KeyPressed,
    KeyReleased,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseWheelScrolled,
    GamepadButtonPressed,
    GamepadButtonReleased,
    GamepadAxisMoved,
    TextEntered
};
```

#### Key Codes

```cpp
enum class KeyCode
{
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    
    // Arrow keys
    Left, Right, Up, Down,
    
    // Special keys
    Space, Enter, Escape, Tab, Backspace, Delete,
    LShift, RShift, LControl, RControl, LAlt, RAlt,
    
    // ... more keys
};
```

#### Mouse Buttons

```cpp
enum class MouseButton
{
    Left,
    Right,
    Middle,
    XButton1,
    XButton2
};
```

#### InputEvent Structure

```cpp
struct InputEvent
{
    InputEventType type;
    
    // Keyboard events
    struct KeyEvent
    {
        KeyCode code;
        bool alt;
        bool control;
        bool shift;
        bool system;
    } key;
    
    // Mouse button events
    struct MouseButtonEvent
    {
        MouseButton button;
        int x;
        int y;
    } mouseButton;
    
    // Mouse move events
    struct MouseMoveEvent
    {
        int x;
        int y;
    } mouseMove;
    
    // Mouse wheel events
    struct MouseWheelEvent
    {
        float delta;
        int x;
        int y;
    } mouseWheel;
    
    // Gamepad events
    struct GamepadButtonEvent
    {
        unsigned int gamepadId;
        unsigned int button;
    } gamepadButton;
    
    struct GamepadAxisEvent
    {
        unsigned int gamepadId;
        unsigned int axis;
        float position;
    } gamepadAxis;
    
    // Text events
    struct TextEvent
    {
        unsigned int unicode;
    } text;
};
```

---

### Usage Examples

#### Keyboard Input

```cpp
void ProcessInput(const Dia::Input::InputEvent& event)
{
    using namespace Dia::Input;
    
    switch (event.type)
    {
        case InputEventType::KeyPressed:
        {
            if (event.key.code == KeyCode::Space)
            {
                Jump();
            }
            else if (event.key.code == KeyCode::Escape)
            {
                Pause();
            }
            
            // Modifier keys
            if (event.key.control && event.key.code == KeyCode::S)
            {
                Save();  // Ctrl+S
            }
            break;
        }
        
        case InputEventType::KeyReleased:
        {
            if (event.key.code == KeyCode::W)
            {
                StopMovingForward();
            }
            break;
        }
    }
}
```

---

#### Mouse Input

```cpp
void ProcessMouseInput(const Dia::Input::InputEvent& event)
{
    using namespace Dia::Input;
    
    switch (event.type)
    {
        case InputEventType::MouseButtonPressed:
        {
            if (event.mouseButton.button == MouseButton::Left)
            {
                OnLeftClick(event.mouseButton.x, event.mouseButton.y);
            }
            else if (event.mouseButton.button == MouseButton::Right)
            {
                OnRightClick(event.mouseButton.x, event.mouseButton.y);
            }
            break;
        }
        
        case InputEventType::MouseMoved:
        {
            OnMouseMove(event.mouseMove.x, event.mouseMove.y);
            break;
        }
        
        case InputEventType::MouseWheelScrolled:
        {
            OnMouseWheel(event.mouseWheel.delta);
            break;
        }
    }
}
```

---

#### Text Input

```cpp
void ProcessTextInput(const Dia::Input::InputEvent& event)
{
    if (event.type == Dia::Input::InputEventType::TextEntered)
    {
        // Convert unicode to char (basic ASCII)
        if (event.text.unicode < 128)
        {
            char character = static_cast<char>(event.text.unicode);
            textBuffer += character;
        }
    }
}
```

---

### InputSourceManager

**Header:** `Dia/DiaInput/DiaInputInputSourceManager.h`

**Purpose:** Manages multiple input sources

#### Key Methods

```cpp
class InputSourceManager
{
public:
    // Add input source
    void AddSource(IInputSource* source);
    void RemoveSource(IInputSource* source);
    
    // Poll events
    bool PollEvent(InputEvent& outEvent);
};
```

#### Usage Example

```cpp
// Create manager
Dia::Input::InputSourceManager inputManager;

// Add SFML input source
Dia::SFML::DiaSFMLInputSource* sfmlInput = new Dia::SFML::DiaSFMLInputSource(window);
inputManager.AddSource(sfmlInput);

// Poll events
Dia::Input::InputEvent event;
while (inputManager.PollEvent(event))
{
    ProcessInput(event);
}
```

---

## Common Patterns

### Event Polling Loop

```cpp
void MainThread::Update()
{
    Dia::Input::InputEvent event;
    while (mInputManager->PollEvent(event))
    {
        // Forward to Sim thread via FrameStream
        mInputFrameStream.Write(event);
        
        // Process immediately on Main thread
        ProcessUIInput(event);
    }
}
```

---

### Input State Tracking

```cpp
class InputState
{
public:
    void ProcessEvent(const Dia::Input::InputEvent& event)
    {
        using namespace Dia::Input;
        
        switch (event.type)
        {
            case InputEventType::KeyPressed:
                mKeys[event.key.code] = true;
                break;
                
            case InputEventType::KeyReleased:
                mKeys[event.key.code] = false;
                break;
                
            case InputEventType::MouseButtonPressed:
                mMouseButtons[event.mouseButton.button] = true;
                break;
                
            case InputEventType::MouseButtonReleased:
                mMouseButtons[event.mouseButton.button] = false;
                break;
                
            case InputEventType::MouseMoved:
                mMouseX = event.mouseMove.x;
                mMouseY = event.mouseMove.y;
                break;
        }
    }
    
    bool IsKeyDown(Dia::Input::KeyCode key) const
    {
        auto it = mKeys.find(key);
        return it != mKeys.end() && it->second;
    }
    
    bool IsMouseButtonDown(Dia::Input::MouseButton button) const
    {
        auto it = mMouseButtons.find(button);
        return it != mMouseButtons.end() && it->second;
    }
    
    void GetMousePosition(int& x, int& y) const
    {
        x = mMouseX;
        y = mMouseY;
    }
    
private:
    std::map<Dia::Input::KeyCode, bool> mKeys;
    std::map<Dia::Input::MouseButton, bool> mMouseButtons;
    int mMouseX = 0;
    int mMouseY = 0;
};
```

---

### Movement Input

```cpp
Dia::Maths::Vector2D GetMovementInput(const InputState& input)
{
    using namespace Dia::Input;
    
    Dia::Maths::Vector2D movement(0.0f, 0.0f);
    
    // WASD movement
    if (input.IsKeyDown(KeyCode::W))
        movement.y -= 1.0f;
    if (input.IsKeyDown(KeyCode::S))
        movement.y += 1.0f;
    if (input.IsKeyDown(KeyCode::A))
        movement.x -= 1.0f;
    if (input.IsKeyDown(KeyCode::D))
        movement.x += 1.0f;
    
    // Normalize diagonal movement
    if (movement.Magnitude() > 0.0f)
    {
        movement = movement.Normalize();
    }
    
    return movement;
}
```

---

### Action Mapping

```cpp
enum class GameAction
{
    Jump,
    Fire,
    Reload,
    Pause
};

class InputMapper
{
public:
    void MapKey(GameAction action, Dia::Input::KeyCode key)
    {
        mKeyMap[key] = action;
    }
    
    bool GetAction(const Dia::Input::InputEvent& event, GameAction& outAction)
    {
        if (event.type == Dia::Input::InputEventType::KeyPressed)
        {
            auto it = mKeyMap.find(event.key.code);
            if (it != mKeyMap.end())
            {
                outAction = it->second;
                return true;
            }
        }
        return false;
    }
    
private:
    std::map<Dia::Input::KeyCode, GameAction> mKeyMap;
};

// Usage
InputMapper mapper;
mapper.MapKey(GameAction::Jump, Dia::Input::KeyCode::Space);
mapper.MapKey(GameAction::Fire, Dia::Input::KeyCode::LControl);
mapper.MapKey(GameAction::Pause, Dia::Input::KeyCode::Escape);

// In event loop
GameAction action;
if (mapper.GetAction(event, action))
{
    switch (action)
    {
        case GameAction::Jump:
            player.Jump();
            break;
        case GameAction::Fire:
            player.Fire();
            break;
        case GameAction::Pause:
            game.Pause();
            break;
    }
}
```

---

## Backend Implementation

### DiaSFML Implementation

**Class:** `Dia::SFML::DiaSFMLInputSource`

**Header:** `Dia/DiaSFML/DiaSFMLInputSource.h`

```cpp
class DiaSFMLInputSource : public Dia::Input::IInputSource
{
public:
    DiaSFMLInputSource(sf::Window* window);
    
    bool PollEvent(Dia::Input::InputEvent& outEvent) override;
    
private:
    sf::Window* mWindow;
    
    // SFML → DiaInput conversion
    Dia::Input::KeyCode ConvertKey(sf::Keyboard::Key key);
    Dia::Input::MouseButton ConvertMouseButton(sf::Mouse::Button button);
};
```

**[→ DiaSFML API Details](sfml-api.md)**

---

## Dependencies

**Required:**
- None (self-contained)

**Implementation:**
- `Dia/DiaSFML/` - SFML backend

---

## Thread Safety

| Component | Thread Safety |
|-----------|---------------|
| `InputEvent` | ✅ Safe (copyable struct) |
| `InputSourceManager::PollEvent()` | ❌ Call from Main thread only |

**Important:** Input events should be **polled on Main thread** and forwarded to other threads via `FrameStream<InputEvent>`.

---

## Best Practices

### 1. Forward Events to Sim Thread

```cpp
// Main thread
Dia::Input::InputEvent event;
while (inputManager.PollEvent(event))
{
    mInputFrameStream.Write(event);  // Forward to Sim
}

// Sim thread
Dia::Input::InputEvent event;
while (mInputFrameStream.Read(event))
{
    ProcessGameInput(event);
}
```

---

### 2. Separate UI and Game Input

```cpp
void ProcessEvent(const Dia::Input::InputEvent& event)
{
    // UI first (on Main thread)
    if (ProcessUIInput(event))
    {
        return;  // UI consumed event
    }
    
    // Forward to game (Sim thread)
    mInputFrameStream.Write(event);
}
```

---

### 3. Use State Tracking for Continuous Input

```cpp
// ✅ Good: Track state for movement
InputState inputState;

void Update()
{
    // Poll events
    while (PollEvent(event))
    {
        inputState.ProcessEvent(event);
    }
    
    // Query state
    if (inputState.IsKeyDown(KeyCode::W))
    {
        MoveForward();  // Continuous movement
    }
}

// ❌ Bad: Try to handle movement in events
void ProcessEvent(const InputEvent& event)
{
    if (event.type == KeyPressed && event.key.code == KeyCode::W)
    {
        MoveForward();  // Only moves once per key press!
    }
}
```

---

### 4. Handle Text Input Separately

```cpp
// Text input for UI fields
if (textFieldActive && event.type == InputEventType::TextEntered)
{
    HandleTextInput(event.text.unicode);
}

// Game input
if (event.type == InputEventType::KeyPressed)
{
    HandleGameInput(event.key.code);
}
```

---

## Gotchas

### Gotcha 1: Event Polling is Destructive

Polling an event **removes it from the queue**. Don't poll twice.

---

### Gotcha 2: Key Repeats

Key events may **repeat** if key held down (OS behavior). Track state if you need "key held" logic.

---

### Gotcha 3: Mouse Coordinates

Mouse coordinates are **window-relative**, not world-relative. Transform if needed.

```cpp
// Window coordinates → World coordinates
Dia::Maths::Vector2D mouseWorld = 
    cameraTransform.Inverse() * 
    Dia::Maths::Vector2D(event.mouseButton.x, event.mouseButton.y);
```

---

### Gotcha 4: Focus Loss

Input events **stop when window loses focus**. Handle focus events if needed.

---

## Limitations

### Current Limitations

1. **No gamepad mapping** - Raw gamepad IDs and buttons
2. **No input recording/playback** - For replays
3. **No gesture support** - Touch gestures
4. **Limited gamepad support** - Basic buttons/axes only

### Future Improvements

- Gamepad abstraction (generic button mapping)
- Input recording system
- Touch/gesture support
- Rumble/haptic feedback

---

## Summary

**Core Types:**
- `InputEvent` - Platform-agnostic input events
- `InputEventType` - Event type enum
- `KeyCode` - Keyboard key enum
- `MouseButton` - Mouse button enum

**Event Types:**
- Keyboard (pressed/released)
- Mouse button (pressed/released)
- Mouse move
- Mouse wheel
- Gamepad button/axis
- Text entry

**Input Flow:**
1. Backend captures events (SFML)
2. Converts to DiaInput format
3. Main thread polls events
4. Forwards to Sim thread via FrameStream

**Thread Safety:**
- ❌ Poll from Main thread only
- ✅ InputEvent copyable, safe to forward

**Best Practices:**
- Track state for continuous input
- Forward events to Sim thread
- Separate UI and game input
- Use action mapping for flexibility

**[→ API Overview](../api-overview.md)**  
**[→ DiaSFML API](sfml-api.md)**  
**[→ DiaApplicationFlow API](application-api.md)**
