---
schema: dia.module.v1
module_id: dia.input
name: Input
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaInput
language: cpp
parent_module_id: dia.root

summary: >
  Defines Input APIs (ConsoleGamepad, ConsoleGamepadManager, EJoystick, EKey, EMouseButton, Event, EventData, IInputSource, InputSourceManager) including: ConsoleGamepad, ConsoleGamepadAnalogueTriggerEvent, ConsoleGamepadButtonEvent, ConsoleGamepadConnectEvent, ConsoleGamepadManager, ConsoleGamepadMoveEvent, Event, EventData, IInputSource, InputSourceManager, JoystickButtonEvent, JoystickConnectEvent.

intent: >
  Provide reusable Input building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: ConsoleGamepad, ConsoleGamepadAnalogueTriggerEvent, ConsoleGamepadButtonEvent, ConsoleGamepadConnectEvent, ConsoleGamepadManager, ConsoleGamepadMoveEvent, Event, EventData, IInputSource, InputSourceManager, JoystickButtonEvent, JoystickConnectEvent
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

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
  namespaces: []
  entry_points:
    - ConsoleGamepad
    - ConsoleGamepadAnalogueTriggerEvent
    - ConsoleGamepadButtonEvent
    - ConsoleGamepadConnectEvent
    - ConsoleGamepadManager
    - ConsoleGamepadMoveEvent
    - Event
    - EventData
    - IInputSource
    - InputSourceManager
    - JoystickButtonEvent
    - JoystickConnectEvent

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
  forbidden: []
---
