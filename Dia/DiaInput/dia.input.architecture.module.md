---
schema: dia.module.v1
module_id: dia.input
name: Input
owner_team: TBD
layer: platform
status: active
maturity: production

path: Dia/DiaInput
language: cpp
parent_module_id: dia.root

summary: >
  Comprehensive input abstraction layer for keyboard, mouse, gamepad, and joystick input with both legacy event-based and modern type-safe APIs. Includes action mapping, input state queries, context-sensitive input, profile management, and recording/playback for testing.

intent: >
  Provide reusable input building blocks with consistent semantics for higher-level systems. Supports multiple input paradigms: event-driven (legacy), type-safe modern events, state queries, action mapping, context stacking, and persistent profiles.

responsibilities:
  - Abstract platform-specific input APIs (XInput, SFML) behind unified interfaces
  - Provide union-based legacy events for backward compatibility
  - Provide type-safe modern events integrated with DiaCore::Events system
  - Support input source prioritization (UI before gameplay)
  - Enable input state queries (IsKeyDown, IsMouseButtonDown, IsJoystickButtonDown)
  - Provide action mapping system for binding inputs to abstract actions
  - Support context-sensitive input (menu vs gameplay contexts)
  - Enable saving/loading of input profiles (key bindings) to JSON
  - Support input recording and playback for testing and replays
  - Support joystick/flight stick input (buttons, axes, hot-plug)
  - Log input events and errors for debugging
  - Handle XInput errors gracefully

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaInput/ConsoleGamepad.h
    - Dia/DiaInput/ConsoleGamepadManager.h
    - Dia/DiaInput/EJoystick.h
    - Dia/DiaInput/EKey.h
    - Dia/DiaInput/EMouseButton.h
    - Dia/DiaInput/Event.h
    - Dia/DiaInput/EventData.h
    - Dia/DiaInput/IInputSource.h
    - Dia/DiaInput/InputSourceManager.h
    - Dia/DiaInput/InputState.h
    - Dia/DiaInput/ActionMap.h
    - Dia/DiaInput/ActionContext.h
    - Dia/DiaInput/InputProfile.h
    - Dia/DiaInput/InputRecorder.h
    - Dia/DiaInput/Events/KeyboardEvents.h
    - Dia/DiaInput/Events/MouseEvents.h
    - Dia/DiaInput/Events/GamepadEvents.h
    - Dia/DiaInput/Events/JoystickEvents.h
    - Dia/DiaInput/Events/LegacyEventConverter.h
  namespaces:
    - Dia::Input
    - Dia::Input::Events
  entry_points:
    - ConsoleGamepad
    - ConsoleGamepadManager
    - IInputSource
    - InputSourceManager
    - Event (legacy union-based)
    - EventData (configurable buffer size)
    - InputState (query API with joystick support)
    - ActionMap (action binding with joystick support)
    - ActionContext (context-sensitive input)
    - ActionContextManager (context stack management)
    - InputProfile (save/load bindings)
    - InputRecorder (recording/playback)
    - PlaybackInputSource
    - KeyPressedEvent (modern)
    - KeyReleasedEvent (modern)
    - MouseMovedEvent (modern)
    - GamepadButtonPressedEvent (modern)
    - JoystickButtonPressedEvent (modern)
    - JoystickAxisMovedEvent (modern)
    - LegacyEventConverter

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.containers.hashtable
    - dia.core.core
    - dia.core.logging
    - dia.core.events
    - dia.core.time
    - dia.core.crc
  forbidden: []
---
