////////////////////////////////////////////////////////////////////////////////
// Filename: TestActionMap.cpp - Google Test for ActionMap
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/ActionMap.h>
#include <DiaInput/Event.h>
#include <DiaInput/EventData.h>
#include <DiaInput/EKey.h>
#include <DiaInput/EMouseButton.h>
#include <DiaInput/ConsoleGamepad.h>

using namespace Dia::Input;

////////////////////////////////////////////////////////////////////////////////
// Basic Binding Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionMap, BindKeyToAction)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);

	// Create key press event
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	actionMap.ProcessEvents(events);

	EXPECT_TRUE(actionMap.IsActionActive(jumpAction));
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(jumpAction), 1.0f);
}

TEST(ActionMap, BindMouseButtonToAction)
{
	ActionMap actionMap;
	ActionID fireAction("Fire");

	actionMap.BindMouseButton(fireAction, EMouseButton::kLeft);

	// Create mouse button press event
	EventData events;
	Event evt;
	evt.type = Event::EType::kMouseButtonPressed;
	evt.mouseButton.button = static_cast<int>(EMouseButton::kLeft); evt.mouseButton.x = 0; evt.mouseButton.y = 0;
	events.Add(evt);

	actionMap.ProcessEvents(events);

	EXPECT_TRUE(actionMap.IsActionActive(fireAction));
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(fireAction), 1.0f);
}

TEST(ActionMap, BindGamepadButtonToAction)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindGamepadButton(jumpAction, 0, ConsoleGamepad::EButtonID::A);

	// Create gamepad button press event
	EventData events;
	Event evt;
	evt.type = Event::EType::kConsoleGamepadButtonPressed;
	evt.consoleGamepadButtonEvent.gamepadIndex = 0;
	evt.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events.Add(evt);

	actionMap.ProcessEvents(events);

	EXPECT_TRUE(actionMap.IsActionActive(jumpAction));
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(jumpAction), 1.0f);
}

TEST(ActionMap, BindJoystickButtonToAction)
{
	ActionMap actionMap;
	ActionID fireAction("Fire");

	actionMap.BindJoystickButton(fireAction, 0, 0);

	// Create joystick button press event
	EventData events;
	Event evt;
	evt.type = Event::EType::kJoystickButtonPressed;
	evt.joystickButton.joystickId = 0;
	evt.joystickButton.button = 0;
	events.Add(evt);

	actionMap.ProcessEvents(events);

	EXPECT_TRUE(actionMap.IsActionActive(fireAction));
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(fireAction), 1.0f);
}

TEST(ActionMap, BindJoystickAxisToAction)
{
	ActionMap actionMap;
	ActionID rollAction("Roll");

	actionMap.BindJoystickAxis(rollAction, 0, EJoystickAxis::X);

	// Create joystick axis move event
	EventData events;
	Event evt;
	evt.type = Event::EType::kJoystickMoved;
	evt.joystickMove.joystickId = 0;
	evt.joystickMove.axis = static_cast<int>(EJoystickAxis::X);
	evt.joystickMove.position = 75.0f;  // SFML uses -100 to 100
	events.Add(evt);

	actionMap.ProcessEvents(events);

	EXPECT_TRUE(actionMap.IsActionActive(rollAction));
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(rollAction), 0.75f);  // Normalized to -1 to 1
}

////////////////////////////////////////////////////////////////////////////////
// Multiple Bindings Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionMap, MultipleBindingsForSameAction)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	// Bind both Space and Gamepad A to Jump
	actionMap.BindKey(jumpAction, EKey::Space);
	actionMap.BindGamepadButton(jumpAction, 0, ConsoleGamepad::EButtonID::A);

	// Press Space
	EventData events1;
	Event spaceEvent;
	spaceEvent.type = Event::EType::kKeyPressed;
	spaceEvent.key.code = static_cast<int>(EKey::Space); spaceEvent.key.alt = false; spaceEvent.key.control = false; spaceEvent.key.shift = false; spaceEvent.key.system = false;
	events1.Add(spaceEvent);

	actionMap.ProcessEvents(events1);
	EXPECT_TRUE(actionMap.IsActionActive(jumpAction));

	// Press Gamepad A
	EventData events2;
	Event gamepadEvent;
	gamepadEvent.type = Event::EType::kConsoleGamepadButtonPressed;
	gamepadEvent.consoleGamepadButtonEvent.gamepadIndex = 0;
	gamepadEvent.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events2.Add(gamepadEvent);

	actionMap.ProcessEvents(events2);
	EXPECT_TRUE(actionMap.IsActionActive(jumpAction));
}

TEST(ActionMap, MultipleActionsSimultaneously)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");
	ActionID fireAction("Fire");
	ActionID moveAction("Move");

	actionMap.BindKey(jumpAction, EKey::Space);
	actionMap.BindMouseButton(fireAction, EMouseButton::kLeft);
	actionMap.BindKey(moveAction, EKey::W);

	// Press all inputs
	EventData events;

	Event spaceEvent;
	spaceEvent.type = Event::EType::kKeyPressed;
	spaceEvent.key.code = static_cast<int>(EKey::Space); spaceEvent.key.alt = false; spaceEvent.key.control = false; spaceEvent.key.shift = false; spaceEvent.key.system = false;
	events.Add(spaceEvent);

	Event mouseEvent;
	mouseEvent.type = Event::EType::kMouseButtonPressed;
	mouseEvent.mouseButton.button = static_cast<int>(EMouseButton::kLeft); mouseEvent.mouseButton.x = 0; mouseEvent.mouseButton.y = 0;
	events.Add(mouseEvent);

	Event wEvent;
	wEvent.type = Event::EType::kKeyPressed;
	wEvent.key.code = static_cast<int>(EKey::W); wEvent.key.alt = false; wEvent.key.control = false; wEvent.key.shift = false; wEvent.key.system = false;
	events.Add(wEvent);

	actionMap.ProcessEvents(events);

	EXPECT_TRUE(actionMap.IsActionActive(jumpAction));
	EXPECT_TRUE(actionMap.IsActionActive(fireAction));
	EXPECT_TRUE(actionMap.IsActionActive(moveAction));
}

////////////////////////////////////////////////////////////////////////////////
// Unbind Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionMap, UnbindAction)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);
	actionMap.UnbindAction(jumpAction);

	// Press Space
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	actionMap.ProcessEvents(events);

	// Action should not be active after unbinding
	EXPECT_FALSE(actionMap.IsActionActive(jumpAction));
}

TEST(ActionMap, ClearAllBindings)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");
	ActionID fireAction("Fire");

	actionMap.BindKey(jumpAction, EKey::Space);
	actionMap.BindMouseButton(fireAction, EMouseButton::kLeft);

	actionMap.ClearAllBindings();

	// Press inputs
	EventData events;

	Event spaceEvent;
	spaceEvent.type = Event::EType::kKeyPressed;
	spaceEvent.key.code = static_cast<int>(EKey::Space); spaceEvent.key.alt = false; spaceEvent.key.control = false; spaceEvent.key.shift = false; spaceEvent.key.system = false;
	events.Add(spaceEvent);

	Event mouseEvent;
	mouseEvent.type = Event::EType::kMouseButtonPressed;
	mouseEvent.mouseButton.button = static_cast<int>(EMouseButton::kLeft); mouseEvent.mouseButton.x = 0; mouseEvent.mouseButton.y = 0;
	events.Add(mouseEvent);

	actionMap.ProcessEvents(events);

	// No actions should be active
	EXPECT_FALSE(actionMap.IsActionActive(jumpAction));
	EXPECT_FALSE(actionMap.IsActionActive(fireAction));
}

////////////////////////////////////////////////////////////////////////////////
// Action Value Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionMap, DigitalInputsReturnOneOrZero)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);

	// Press Space
	EventData pressEvents;
	Event pressEvent;
	pressEvent.type = Event::EType::kKeyPressed;
	pressEvent.key.code = static_cast<int>(EKey::Space); pressEvent.key.alt = false; pressEvent.key.control = false; pressEvent.key.shift = false; pressEvent.key.system = false;
	pressEvents.Add(pressEvent);

	actionMap.ProcessEvents(pressEvents);
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(jumpAction), 1.0f);

	// Release Space
	EventData releaseEvents;
	Event releaseEvent;
	releaseEvent.type = Event::EType::kKeyReleased;
	releaseEvent.key.code = static_cast<int>(EKey::Space); releaseEvent.key.alt = false; releaseEvent.key.control = false; releaseEvent.key.shift = false; releaseEvent.key.system = false;
	releaseEvents.Add(releaseEvent);

	actionMap.ProcessEvents(releaseEvents);
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(jumpAction), 0.0f);
}

TEST(ActionMap, AnalogInputsReturnNormalizedValue)
{
	ActionMap actionMap;
	ActionID rollAction("Roll");

	actionMap.BindJoystickAxis(rollAction, 0, EJoystickAxis::X);

	// Move axis to 50%
	EventData events;
	Event evt;
	evt.type = Event::EType::kJoystickMoved;
	evt.joystickMove.joystickId = 0;
	evt.joystickMove.axis = static_cast<int>(EJoystickAxis::X);
	evt.joystickMove.position = 50.0f;  // SFML uses -100 to 100
	events.Add(evt);

	actionMap.ProcessEvents(events);

	// Should be normalized to 0.5
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(rollAction), 0.5f);
}

TEST(ActionMap, InactiveActionReturnsZero)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);

	// Don't press anything
	EventData events;
	actionMap.ProcessEvents(events);

	EXPECT_FALSE(actionMap.IsActionActive(jumpAction));
	EXPECT_FLOAT_EQ(actionMap.GetActionValue(jumpAction), 0.0f);
}

////////////////////////////////////////////////////////////////////////////////
// Callback Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionMap, ActionActivatedCallback)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);

	// Subscribe to callback
	bool callbackInvoked = false;
	ActionID receivedAction;
	float receivedValue = 0.0f;

	actionMap.OnActionActivated([&](ActionID action, float value) {
		callbackInvoked = true;
		receivedAction = action;
		receivedValue = value;
	});

	// Press Space
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	actionMap.ProcessEvents(events);

	EXPECT_TRUE(callbackInvoked);
	EXPECT_EQ(receivedAction, jumpAction);
	EXPECT_FLOAT_EQ(receivedValue, 1.0f);
}

TEST(ActionMap, MultipleCallbacksInvoked)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);

	// Subscribe multiple callbacks
	int callback1Count = 0;
	int callback2Count = 0;

	actionMap.OnActionActivated([&](ActionID action, float value) {
		callback1Count++;
	});

	actionMap.OnActionActivated([&](ActionID action, float value) {
		callback2Count++;
	});

	// Press Space
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key.code = static_cast<int>(EKey::Space); evt.key.alt = false; evt.key.control = false; evt.key.shift = false; evt.key.system = false;
	events.Add(evt);

	actionMap.ProcessEvents(events);

	EXPECT_EQ(callback1Count, 1);
	EXPECT_EQ(callback2Count, 1);
}

TEST(ActionMap, RemoveCallback)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);

	// Subscribe to callback
	int callbackCount = 0;
	auto callbackId = actionMap.OnActionActivated([&](ActionID action, float value) {
		callbackCount++;
	});

	// Press Space
	EventData events1;
	Event evt1;
	evt1.type = Event::EType::kKeyPressed;
	evt1.key.code = static_cast<int>(EKey::Space);
	evt1.key.alt = false;
	evt1.key.control = false;
	evt1.key.shift = false;
	evt1.key.system = false;
	events1.Add(evt1);

	actionMap.ProcessEvents(events1);
	EXPECT_EQ(callbackCount, 1);

	// Remove callback
	actionMap.RemoveCallback(callbackId);

	// Press Space again
	EventData events2;
	Event evt2;
	evt2.type = Event::EType::kKeyPressed;
	evt2.key.code = static_cast<int>(EKey::Space);
	evt2.key.alt = false;
	evt2.key.control = false;
	evt2.key.shift = false;
	evt2.key.system = false;
	events2.Add(evt2);

	actionMap.ProcessEvents(events2);

	// Callback should not be invoked again
	EXPECT_EQ(callbackCount, 1);
}

////////////////////////////////////////////////////////////////////////////////
// Device Index Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionMap, GamepadIndexMatching)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	// Bind to gamepad 0
	actionMap.BindGamepadButton(jumpAction, 0, ConsoleGamepad::EButtonID::A);

	// Press A on gamepad 1 (different gamepad)
	EventData events;
	Event evt;
	evt.type = Event::EType::kConsoleGamepadButtonPressed;
	evt.consoleGamepadButtonEvent.gamepadIndex = 1;
	evt.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events.Add(evt);

	actionMap.ProcessEvents(events);

	// Action should not be active (wrong gamepad)
	EXPECT_FALSE(actionMap.IsActionActive(jumpAction));
}

TEST(ActionMap, JoystickIndexMatching)
{
	ActionMap actionMap;
	ActionID fireAction("Fire");

	// Bind to joystick 0, button 0
	actionMap.BindJoystickButton(fireAction, 0, 0);

	// Press button 0 on joystick 1 (different joystick)
	EventData events;
	Event evt;
	evt.type = Event::EType::kJoystickButtonPressed;
	evt.joystickButton.joystickId = 1;
	evt.joystickButton.button = 0;
	events.Add(evt);

	actionMap.ProcessEvents(events);

	// Action should not be active (wrong joystick)
	EXPECT_FALSE(actionMap.IsActionActive(fireAction));
}

////////////////////////////////////////////////////////////////////////////////
// Frame-to-Frame State Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionMap, ActionValuesResetEachFrame)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	actionMap.BindKey(jumpAction, EKey::Space);

	// Frame 1: Press Space
	EventData events1;
	Event pressEvent;
	pressEvent.type = Event::EType::kKeyPressed;
	pressEvent.key.code = static_cast<int>(EKey::Space); pressEvent.key.alt = false; pressEvent.key.control = false; pressEvent.key.shift = false; pressEvent.key.system = false;
	events1.Add(pressEvent);

	actionMap.ProcessEvents(events1);
	EXPECT_TRUE(actionMap.IsActionActive(jumpAction));

	// Frame 2: No events (key is still held, but ActionMap doesn't maintain state across frames)
	EventData events2;
	actionMap.ProcessEvents(events2);

	// Action should not be active (ActionMap processes events, not state)
	EXPECT_FALSE(actionMap.IsActionActive(jumpAction));
}
