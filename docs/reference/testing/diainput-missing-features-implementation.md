# DiaInput Missing Features Implementation

**Date:** 2026-04-11  
**Status:** ✅ Complete  
**Features Implemented:** 3 major features (Joystick support, Input profiles, Context-sensitive input)  
**Total Tasks:** 9 tasks completed  
**New Files:** 3 files created

---

## Executive Summary

Successfully implemented the 3 most impactful missing features for the DiaInput module:

1. ✅ **Complete Joystick Support** - Modern events, state tracking, action bindings
2. ✅ **Input Profiles** - Save/load key bindings to JSON
3. ✅ **Context-Sensitive Input** - Context stacking for menu vs gameplay

These features transform DiaInput from a keyboard/mouse/gamepad-only system into a comprehensive input abstraction with flight stick support, persistent configuration, and modal input handling.

---

## Feature 1: Complete Joystick Support ✅

### Overview
Added full modern support for joysticks/flight sticks including events, state tracking, and action bindings.

### What Was Added

#### 1.1 Modern Joystick Events
**New File:** `Events/JoystickEvents.h`

**Event Classes:**
- `JoystickButtonPressedEvent` - Button press with joystick ID and button index
- `JoystickButtonReleasedEvent` - Button release
- `JoystickAxisMovedEvent` - Axis movement with EJoystickAxis (X, Y, Z, R, U, V, PovX, PovY)
- `JoystickConnectedEvent` - Hot-plug detection
- `JoystickDisconnectedEvent` - Unplugged detection

**Example:**
```cpp
dispatcher.Subscribe<JoystickButtonPressedEvent>([](auto* evt) {
    if (evt->GetButton() == 0)  // Trigger button
        FireWeapon();
});

dispatcher.Subscribe<JoystickAxisMovedEvent>([](auto* evt) {
    if (evt->GetAxis() == EJoystickAxis::X)
        AileronControl(evt->GetPosition());
});
```

---

#### 1.2 Legacy Event Conversion
**Modified File:** `Events/LegacyEventConverter.h`

**Added Support For:**
- `kJoystickButtonPressed` → `JoystickButtonPressedEvent`
- `kJoystickButtonReleased` → `JoystickButtonReleasedEvent`
- `kJoystickMoved` → `JoystickAxisMovedEvent`
- `kJoystickConnected` → `JoystickConnectedEvent`
- `kJoystickDisconnected` → `JoystickDisconnectedEvent`

Now legacy joystick events from SFML are converted to modern typed events.

---

#### 1.3 InputState Joystick Tracking
**Modified File:** `InputState.h`

**New Constants:**
```cpp
static const unsigned int kMaxJoysticks = 8;
static const unsigned int kMaxJoystickButtons = 32;
static const unsigned int kMaxJoystickAxes = 8;
```

**New Query Methods:**
```cpp
bool IsJoystickConnected(unsigned int index) const;
bool IsJoystickButtonDown(unsigned int index, unsigned int button) const;
float GetJoystickAxisValue(unsigned int index, EJoystickAxis axis) const;
void GetJoystickPosition(unsigned int index, float& x, float& y) const;
```

**Usage:**
```cpp
InputState state;
state.ProcessEvents(events);

if (state.IsJoystickConnected(0))
{
    if (state.IsJoystickButtonDown(0, 0))  // Trigger button
        FireWeapon();
    
    float roll = state.GetJoystickAxisValue(0, EJoystickAxis::X);
    AileronControl(roll);
}
```

---

#### 1.4 ActionMap Joystick Bindings
**Modified File:** `ActionMap.h`

**New Binding Types:**
```cpp
enum class Type
{
    Key,
    MouseButton,
    GamepadButton,
    GamepadAxis,
    JoystickButton,  // NEW
    JoystickAxis     // NEW
};
```

**New Binding Methods:**
```cpp
void BindJoystickButton(ActionID action, unsigned int joystickIndex, unsigned int button);
void BindJoystickAxis(ActionID action, unsigned int joystickIndex, EJoystickAxis axis);
```

**Usage:**
```cpp
ActionMap actionMap;

// Bind joystick trigger to "Fire" action
actionMap.BindJoystickButton(ActionID("Fire"), 0, 0);  // Joystick 0, button 0

// Bind joystick X axis to "Roll" action
actionMap.BindJoystickAxis(ActionID("Roll"), 0, EJoystickAxis::X);

// Process events
actionMap.ProcessEvents(events);

// Query actions
if (actionMap.IsActionActive(ActionID("Fire")))
    FireWeapon();

float rollValue = actionMap.GetActionValue(ActionID("Roll"));  // -1.0 to 1.0
AileronControl(rollValue);
```

---

### Benefits

✅ **Complete Joystick Support** - No longer limited to keyboard/mouse/gamepad  
✅ **Modern Type-Safe Events** - Joysticks integrated with EventDispatcher  
✅ **State Queries** - "Is joystick button down?" alongside keyboard/mouse  
✅ **Action Mapping** - Bind joystick axes/buttons to abstract actions  
✅ **Flight Sim Ready** - 8 axes (X, Y, Z, R, U, V, PovX, PovY) supported  
✅ **Hot-Plug Detection** - Connect/disconnect events for joysticks  

---

## Feature 2: Input Profiles (Save/Load Bindings) ✅

### Overview
Added JSON-based serialization for ActionMap bindings, allowing users to save custom key configurations and load them across sessions.

### What Was Added

#### 2.1 InputProfile Class
**New File:** `InputProfile.h`

**Static Methods:**
```cpp
static bool SaveProfile(const ActionMap& actionMap, const char* filePath, const char* profileName = "Default");
static bool LoadProfile(ActionMap& actionMap, const char* filePath);
static bool GetProfileName(const char* filePath, const char*& outProfileName);
```

**JSON Format:**
```json
{
    "profile_name": "Player1",
    "bindings": [
        {
            "action": "Jump",
            "type": "Key",
            "code": 57,
            "device": 0
        },
        {
            "action": "Jump",
            "type": "GamepadButton",
            "code": 0,
            "device": 0
        },
        {
            "action": "Fire",
            "type": "JoystickButton",
            "code": 0,
            "device": 0
        }
    ]
}
```

**Features:**
- Saves all binding types (Key, MouseButton, GamepadButton, JoystickButton, JoystickAxis)
- Human-readable JSON format
- Profile name metadata
- Multiple bindings per action supported
- Device index preserved

---

#### 2.2 ActionMap Integration
**Modified File:** `ActionMap.h`

**New Methods:**
```cpp
void ClearAllBindings();  // Clear all bindings
const HashTable<ActionID, DynamicArrayC<Binding, 4>>& GetBindings() const;  // Get bindings for serialization
bool SaveProfile(const char* filePath, const char* profileName = "Default") const;  // Convenience method
bool LoadProfile(const char* filePath);  // Convenience method
```

**Usage:**
```cpp
ActionMap actionMap;

// Setup bindings
actionMap.BindKey(ActionID("Jump"), EKey::Space);
actionMap.BindGamepadButton(ActionID("Jump"), 0, ConsoleGamepad::EButtonID::A);
actionMap.BindJoystickButton(ActionID("Fire"), 0, 0);

// Save profile
actionMap.SaveProfile("profiles/player1.json", "Player1");

// Later...load profile
ActionMap loadedMap;
loadedMap.LoadProfile("profiles/player1.json");

// Or use static methods for full control
#include "DiaInput/InputProfile.h"
InputProfile::SaveProfile(actionMap, "profiles/player1.json", "Player1");
InputProfile::LoadProfile(loadedMap, "profiles/player1.json");
```

---

### Benefits

✅ **Persistent Configuration** - Save/load key bindings across sessions  
✅ **Multiple Profiles** - Different players can have different control schemes  
✅ **User Customization** - Allow players to remap keys and save preferences  
✅ **Preset Schemes** - Ship with "WASD", "Arrow Keys", "Gamepad" presets  
✅ **JSON Format** - Human-readable, easy to edit manually  
✅ **All Input Types** - Supports keyboard, mouse, gamepad, joystick  

---

## Feature 3: Context-Sensitive Input ✅

### Overview
Added context stacking system for managing input across different game states (menu vs gameplay vs inventory).

### What Was Added

#### 3.1 ActionContext Class
**New File:** `ActionContext.h`

**ActionContext:**
```cpp
class ActionContext
{
public:
    ActionContext(ContextID contextId, bool blockLower = false);
    
    ContextID GetContextId() const;
    bool BlocksLowerContexts() const;
    void SetBlockLowerContexts(bool block);
    
    ActionMap& GetActionMap();
    
    // Convenience binding methods (forward to internal ActionMap)
    void BindKey(ActionID action, EKey key);
    void BindMouseButton(ActionID action, EMouseButton button);
    void BindGamepadButton(ActionID action, unsigned int gamepadIndex, ConsoleGamepad::EButtonID button);
    void BindJoystickButton(ActionID action, unsigned int joystickIndex, unsigned int button);
    void BindJoystickAxis(ActionID action, unsigned int joystickIndex, EJoystickAxis axis);
    void UnbindAction(ActionID action);
};
```

**Blocking Behavior:**
- If `blockLower = true`, lower contexts don't receive input
- If `blockLower = false`, input passes through to lower contexts
- Modal dialogs should use `blockLower = true`
- Non-modal overlays can use `blockLower = false`

---

#### 3.2 ActionContextManager
**New File:** `ActionContext.h`

**ActionContextManager:**
```cpp
class ActionContextManager
{
public:
    // Create and manage contexts
    ActionContext* CreateContext(ContextID contextId, bool blockLower = false);
    ActionContext* GetContext(ContextID contextId);
    
    // Stack management
    void PushContext(ActionContext* context);
    ActionContext* PopContext();
    ActionContext* GetActiveContext();
    void ClearStack();
    unsigned int GetStackSize() const;
    
    // Input processing
    void ProcessEvents(const EventData& events);
    bool IsActionActive(ActionID action) const;
    float GetActionValue(ActionID action) const;
};
```

**Context Stack Processing:**
1. Processes contexts from top to bottom (LIFO)
2. Respects blocking behavior
3. Higher contexts override lower contexts

---

### Usage Examples

#### Example 1: Menu Over Gameplay (Blocking)
```cpp
ActionContextManager manager;

// Create contexts
ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
gameplay->BindKey(ActionID("Jump"), EKey::Space);
gameplay->BindKey(ActionID("Fire"), EKey::MouseLeft);
gameplay->BindKey(ActionID("Pause"), EKey::Escape);

ActionContext* menu = manager.CreateContext(ContextID("Menu"), true);  // Blocks gameplay
menu->BindKey(ActionID("Navigate"), EKey::Up);
menu->BindKey(ActionID("Navigate"), EKey::Down);
menu->BindKey(ActionID("Select"), EKey::Enter);
menu->BindKey(ActionID("Cancel"), EKey::Escape);

// Start in gameplay
manager.PushContext(gameplay);

// Player presses Escape -> open menu
manager.PushContext(menu);  // Menu blocks gameplay input

// Each frame:
EventData events;
inputSourceManager.Update(events);
manager.ProcessEvents(events);

// In menu mode: only menu actions work
if (manager.IsActionActive(ActionID("Navigate")))
    MoveMenuCursor();

if (manager.IsActionActive(ActionID("Cancel")))
    manager.PopContext();  // Back to gameplay

// Jump/Fire actions are blocked by menu
```

---

#### Example 2: Inventory Overlay (Non-Blocking)
```cpp
ActionContextManager manager;

ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
gameplay->BindKey(ActionID("Move"), EKey::W);
gameplay->BindKey(ActionID("OpenInventory"), EKey::I);

ActionContext* inventory = manager.CreateContext(ContextID("Inventory"), false);  // Doesn't block gameplay
inventory->BindKey(ActionID("SelectItem"), EKey::MouseLeft);
inventory->BindKey(ActionID("DropItem"), EKey::X);

// Start in gameplay
manager.PushContext(gameplay);

// Player presses I -> open inventory overlay
manager.PushContext(inventory);

// Each frame: both contexts process input
manager.ProcessEvents(events);

// Inventory actions work
if (manager.IsActionActive(ActionID("SelectItem")))
    EquipItem();

// Gameplay actions still work (not blocked)
if (manager.IsActionActive(ActionID("Move")))
    player.MoveForward();  // Can still move while inventory is open
```

---

#### Example 3: Switching Contexts
```cpp
ActionContextManager manager;

ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
ActionContext* driving = manager.CreateContext(ContextID("Driving"), false);

// Setup bindings
gameplay->BindKey(ActionID("Jump"), EKey::Space);
driving->BindKey(ActionID("Accelerate"), EKey::W);
driving->BindKey(ActionID("Brake"), EKey::S);

// Start in gameplay
manager.PushContext(gameplay);

// Player enters vehicle
manager.PopContext();  // Remove gameplay
manager.PushContext(driving);  // Add driving

// Player exits vehicle
manager.PopContext();  // Remove driving
manager.PushContext(gameplay);  // Back to gameplay
```

---

### Benefits

✅ **Modal Input Handling** - Menus can block gameplay input  
✅ **Context Stacking** - Hierarchical input (UI → Game → Background)  
✅ **Clean Separation** - Different bindings for different game modes  
✅ **Automatic Priority** - Top context processed first  
✅ **No Code Changes** - Switch contexts without rebinding actions  
✅ **Flexible Blocking** - Choose whether contexts block lower contexts  

---

## Files Changed Summary

### New Files (3)
1. **Events/JoystickEvents.h** - Modern joystick event classes
2. **InputProfile.h** - JSON serialization for ActionMap bindings
3. **ActionContext.h** - Context-sensitive input management

### Modified Files (5)
1. **Events/LegacyEventConverter.h** - Added joystick event conversion
2. **InputState.h** - Added joystick state tracking and queries
3. **ActionMap.h** - Added joystick bindings, profile methods, GetBindings(), ClearAllBindings()
4. **DiaInput.vcxproj** - Added new files to project
5. **dia.input.architecture.module.md** - Updated documentation

---

## Usage Quick Reference

### Joystick Support
```cpp
// Modern events
dispatcher.Subscribe<JoystickButtonPressedEvent>([](auto* evt) { ... });

// State queries
if (state.IsJoystickButtonDown(0, 0))
    Fire();

// Action bindings
actionMap.BindJoystickButton(ActionID("Fire"), 0, 0);
actionMap.BindJoystickAxis(ActionID("Roll"), 0, EJoystickAxis::X);
```

### Input Profiles
```cpp
// Save
actionMap.SaveProfile("player1.json", "Player1");

// Load
actionMap.LoadProfile("player1.json");

// Or use static methods
#include "DiaInput/InputProfile.h"
InputProfile::SaveProfile(actionMap, "player1.json", "Player1");
InputProfile::LoadProfile(actionMap, "player1.json");
```

### Context-Sensitive Input
```cpp
ActionContextManager manager;

// Create contexts
ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
ActionContext* menu = manager.CreateContext(ContextID("Menu"), true);

// Bind actions
gameplay->BindKey(ActionID("Jump"), EKey::Space);
menu->BindKey(ActionID("Select"), EKey::Enter);

// Stack management
manager.PushContext(gameplay);
manager.PushContext(menu);  // Menu active, blocks gameplay

// Process input
manager.ProcessEvents(events);

// Query actions
if (manager.IsActionActive(ActionID("Select")))
    SelectMenuItem();

// Pop context
manager.PopContext();  // Back to gameplay
```

---

## Integration with Existing Features

### Works With Modern Events
All 3 features integrate seamlessly with the modern event system from the original implementation:
```cpp
// Joystick events → EventDispatcher
inputSourceManager.UpdateModern(dispatcher);
dispatcher.Subscribe<JoystickButtonPressedEvent>([](auto* evt) { ... });

// Action bindings → Profiles
actionMap.BindJoystickButton(ActionID("Fire"), 0, 0);
actionMap.SaveProfile("profile.json");

// Contexts → Modern events
manager.ProcessEvents(events);
```

### Works With Input Recording
Joystick events are recorded and played back:
```cpp
InputRecorder recorder;
recorder.StartRecording();
recorder.RecordEvents(events, TimeAbsolute::Now());  // Includes joystick events
recorder.SaveToFile("replay.input");

recorder.LoadFromFile("replay.input");
recorder.StartPlayback();
```

### Works With Priority System
Contexts work with input source priority:
```cpp
// High-priority UI input source
uiInputSource.SetPriority(IInputSource::Priority::High);

// Context manager processes prioritized events
manager.ProcessEvents(events);
```

---

## What's Still Missing

After implementing these 3 features, the following are the most impactful remaining missing features:

### High Priority
1. **Chord Bindings** - Multi-key bindings (Ctrl+S, Shift+Space)
2. **Combo Detection** - Fighting game-style input sequences (←↓→+A)
3. **Analog Sensitivity Curves** - Customizable deadzone and acceleration curves

### Medium Priority
4. **Input Blocking API** - Disable all input during cutscenes
5. **Dead Input Detection** - Idle timeout for "Press any key to continue"
6. **Touch/Multi-Touch** - If targeting mobile/tablets

### Low Priority
7. **Input Prediction** - For networked games
8. **Accessibility Features** - Sticky keys, toggle hold, slow keys
9. **Macro Recording** - Record and bind input sequences to single key

---

## Testing Recommendations

### Unit Tests
```cpp
// Test joystick state tracking
InputState state;
Event evt;
evt.type = Event::EType::kJoystickButtonPressed;
evt.joystickButton.joystickId = 0;
evt.joystickButton.button = 0;
state.ProcessEvents(EventData{evt});
ASSERT_TRUE(state.IsJoystickButtonDown(0, 0));

// Test profile save/load
ActionMap map;
map.BindKey(ActionID("Jump"), EKey::Space);
map.SaveProfile("test.json");

ActionMap loaded;
loaded.LoadProfile("test.json");
ASSERT_TRUE(loaded.GetBindings().Contains(ActionID("Jump")));

// Test context stacking
ActionContextManager manager;
ActionContext* ctx1 = manager.CreateContext(ContextID("Ctx1"), false);
ActionContext* ctx2 = manager.CreateContext(ContextID("Ctx2"), true);
manager.PushContext(ctx1);
manager.PushContext(ctx2);
ASSERT_EQ(manager.GetStackSize(), 2);
manager.PopContext();
ASSERT_EQ(manager.GetActiveContext(), ctx1);
```

### Integration Tests
- Joystick hot-plug detection
- Profile load/save round-trip
- Context switching during gameplay
- Multi-context input processing

---

## Migration Guide

### For Existing Code Using ActionMap
```cpp
// Old code still works
ActionMap map;
map.BindKey(ActionID("Jump"), EKey::Space);
map.ProcessEvents(events);

// New features are opt-in
map.BindJoystickButton(ActionID("Fire"), 0, 0);  // NEW
map.SaveProfile("profile.json");  // NEW

// Or use contexts
ActionContextManager manager;
ActionContext* ctx = manager.CreateContext(ContextID("Gameplay"));
ctx->BindKey(ActionID("Jump"), EKey::Space);
manager.PushContext(ctx);
manager.ProcessEvents(events);  // NEW API
```

---

## Conclusion

Successfully implemented the 3 most impactful missing features for DiaInput:

✅ **Joystick Support** - Complete modern integration (events, state, bindings)  
✅ **Input Profiles** - JSON-based save/load for persistent configuration  
✅ **Context-Sensitive Input** - Stack-based context management for modal input  

The DiaInput module now supports:
- **5 input device types** (keyboard, mouse, gamepad, joystick, touch-future)
- **2 event paradigms** (legacy union, modern typed)
- **3 query methods** (events, state queries, action mapping)
- **Context stacking** (menu vs gameplay vs inventory)
- **Persistent profiles** (save/load bindings)
- **Recording/playback** (testing and replays)

The module is now feature-complete for most game development scenarios! 🎮✨
