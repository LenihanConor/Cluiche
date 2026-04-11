////////////////////////////////////////////////////////////////////////////////
// Filename: TestInputState.cpp - Google Test for InputState
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/InputState.h>
#include <DiaInput/Event.h>
#include <DiaInput/EventData.h>
#include <DiaInput/EKey.h>
#include <DiaInput/EMouseButton.h>
#include <DiaInput/ConsoleGamepad.h>

using namespace Dia::Input;

////////////////////////////////////////////////////////////////////////////////
// Keyboard State Tests
////////////////////////////////////////////////////////////////////////////////

TEST(InputState, KeyPressedAndReleased)
{
	InputState state;
	EventData events;

	// Create key pressed event
	Event pressEvent;
	pressEvent.type = Event::EType::kKeyPressed;
	pressEvent.key = static_cast<int>(EKey::W);
	events.Add(pressEvent);

	state.ProcessEvents(events);

	// Key should be down
	EXPECT_TRUE(state.IsKeyDown(EKey::W));
	EXPECT_TRUE(state.WasKeyPressed(EKey::W));
	EXPECT_FALSE(state.WasKeyReleased(EKey::W));

	// Clear frame state and process release
	state.ClearFrameState();
	events.RemoveAll();

	Event releaseEvent;
	releaseEvent.type = Event::EType::kKeyReleased;
	releaseEvent.key = static_cast<int>(EKey::W);
	events.Add(releaseEvent);

	state.ProcessEvents(events);

	// Key should no longer be down
	EXPECT_FALSE(state.IsKeyDown(EKey::W));
	EXPECT_FALSE(state.WasKeyPressed(EKey::W));
	EXPECT_TRUE(state.WasKeyReleased(EKey::W));
}

TEST(InputState, MultipleKeysDown)
{
	InputState state;
	EventData events;

	// Press multiple keys
	Event wPress;
	wPress.type = Event::EType::kKeyPressed;
	wPress.key = static_cast<int>(EKey::W);
	events.Add(wPress);

	Event aPress;
	aPress.type = Event::EType::kKeyPressed;
	aPress.key = static_cast<int>(EKey::A);
	events.Add(aPress);

	Event spacePress;
	spacePress.type = Event::EType::kKeyPressed;
	spacePress.key = static_cast<int>(EKey::Space);
	events.Add(spacePress);

	state.ProcessEvents(events);

	EXPECT_TRUE(state.IsKeyDown(EKey::W));
	EXPECT_TRUE(state.IsKeyDown(EKey::A));
	EXPECT_TRUE(state.IsKeyDown(EKey::Space));
	EXPECT_FALSE(state.IsKeyDown(EKey::S));
}

TEST(InputState, KeyStateAcrossFrames)
{
	InputState state;
	EventData events;

	// Frame 1: Press W
	Event pressEvent;
	pressEvent.type = Event::EType::kKeyPressed;
	pressEvent.key = static_cast<int>(EKey::W);
	events.Add(pressEvent);
	state.ProcessEvents(events);

	EXPECT_TRUE(state.WasKeyPressed(EKey::W));

	// Frame 2: Key still down but no new press event
	state.ClearFrameState();
	events.RemoveAll();
	state.ProcessEvents(events);

	EXPECT_TRUE(state.IsKeyDown(EKey::W));  // Still held
	EXPECT_FALSE(state.WasKeyPressed(EKey::W));  // Not pressed this frame
}

////////////////////////////////////////////////////////////////////////////////
// Mouse State Tests
////////////////////////////////////////////////////////////////////////////////

TEST(InputState, MouseButtonPressedAndReleased)
{
	InputState state;
	EventData events;

	// Press left mouse button
	Event pressEvent;
	pressEvent.type = Event::EType::kMouseButtonPressed;
	pressEvent.mouseButton = static_cast<int>(EMouseButton::Left);
	events.Add(pressEvent);

	state.ProcessEvents(events);

	EXPECT_TRUE(state.IsMouseButtonDown(EMouseButton::Left));
	EXPECT_FALSE(state.IsMouseButtonDown(EMouseButton::Right));

	// Release left mouse button
	state.ClearFrameState();
	events.RemoveAll();

	Event releaseEvent;
	releaseEvent.type = Event::EType::kMouseButtonReleased;
	releaseEvent.mouseButton = static_cast<int>(EMouseButton::Left);
	events.Add(releaseEvent);

	state.ProcessEvents(events);

	EXPECT_FALSE(state.IsMouseButtonDown(EMouseButton::Left));
}

TEST(InputState, MousePosition)
{
	InputState state;
	EventData events;

	// Move mouse to (100, 200)
	Event moveEvent;
	moveEvent.type = Event::EType::kMouseMoved;
	moveEvent.mouseMove.x = 100;
	moveEvent.mouseMove.y = 200;
	events.Add(moveEvent);

	state.ProcessEvents(events);

	int x, y;
	state.GetMousePosition(x, y);

	EXPECT_EQ(x, 100);
	EXPECT_EQ(y, 200);
}

TEST(InputState, MousePositionUpdates)
{
	InputState state;
	EventData events;

	// Move mouse to (50, 75)
	Event move1;
	move1.type = Event::EType::kMouseMoved;
	move1.mouseMove.x = 50;
	move1.mouseMove.y = 75;
	events.Add(move1);

	// Move mouse to (150, 225)
	Event move2;
	move2.type = Event::EType::kMouseMoved;
	move2.mouseMove.x = 150;
	move2.mouseMove.y = 225;
	events.Add(move2);

	state.ProcessEvents(events);

	int x, y;
	state.GetMousePosition(x, y);

	// Should have latest position
	EXPECT_EQ(x, 150);
	EXPECT_EQ(y, 225);
}

////////////////////////////////////////////////////////////////////////////////
// Gamepad State Tests
////////////////////////////////////////////////////////////////////////////////

TEST(InputState, GamepadConnectedAndDisconnected)
{
	InputState state;
	EventData events;

	// Connect gamepad 0
	Event connectEvent;
	connectEvent.type = Event::EType::kConsoleGamepadConnected;
	connectEvent.consoleGamepadConnectionEvent.gamepadIndex = 0;
	events.Add(connectEvent);

	state.ProcessEvents(events);

	EXPECT_TRUE(state.IsGamepadConnected(0));
	EXPECT_FALSE(state.IsGamepadConnected(1));

	// Disconnect gamepad 0
	state.ClearFrameState();
	events.RemoveAll();

	Event disconnectEvent;
	disconnectEvent.type = Event::EType::kConsoleGamepadDisconnected;
	disconnectEvent.consoleGamepadConnectionEvent.gamepadIndex = 0;
	events.Add(disconnectEvent);

	state.ProcessEvents(events);

	EXPECT_FALSE(state.IsGamepadConnected(0));
}

TEST(InputState, GamepadButtonState)
{
	InputState state;
	EventData events;

	// Connect gamepad
	Event connectEvent;
	connectEvent.type = Event::EType::kConsoleGamepadConnected;
	connectEvent.consoleGamepadConnectionEvent.gamepadIndex = 0;
	events.Add(connectEvent);

	// Press A button
	Event pressEvent;
	pressEvent.type = Event::EType::kConsoleGamepadButtonPressed;
	pressEvent.consoleGamepadButtonEvent.gamepadIndex = 0;
	pressEvent.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events.Add(pressEvent);

	state.ProcessEvents(events);

	EXPECT_TRUE(state.IsGamepadButtonDown(0, ConsoleGamepad::EButtonID::A));
	EXPECT_FALSE(state.IsGamepadButtonDown(0, ConsoleGamepad::EButtonID::B));
}

TEST(InputState, GamepadAnalogStick)
{
	InputState state;
	EventData events;

	// Connect gamepad
	Event connectEvent;
	connectEvent.type = Event::EType::kConsoleGamepadConnected;
	connectEvent.consoleGamepadConnectionEvent.gamepadIndex = 0;
	events.Add(connectEvent);

	// Move left stick
	Event stickEvent;
	stickEvent.type = Event::EType::kConsoleGamepadAnalogStickMoved;
	stickEvent.consoleGamepadStickEvent.gamepadIndex = 0;
	stickEvent.consoleGamepadStickEvent.stick = static_cast<int>(ConsoleGamepad::EButtonID::AnalogStickLeft);
	stickEvent.consoleGamepadStickEvent.xValue = 0.5f;
	stickEvent.consoleGamepadStickEvent.yValue = -0.75f;
	events.Add(stickEvent);

	state.ProcessEvents(events);

	float x, y;
	state.GetGamepadAnalogStick(0, ConsoleGamepad::EButtonID::AnalogStickLeft, x, y);

	EXPECT_FLOAT_EQ(x, 0.5f);
	EXPECT_FLOAT_EQ(y, -0.75f);
}

////////////////////////////////////////////////////////////////////////////////
// Joystick State Tests
////////////////////////////////////////////////////////////////////////////////

TEST(InputState, JoystickConnectedAndDisconnected)
{
	InputState state;
	EventData events;

	// Connect joystick 0
	Event connectEvent;
	connectEvent.type = Event::EType::kJoystickConnected;
	connectEvent.joystickConnect.joystickId = 0;
	events.Add(connectEvent);

	state.ProcessEvents(events);

	EXPECT_TRUE(state.IsJoystickConnected(0));
	EXPECT_FALSE(state.IsJoystickConnected(1));

	// Disconnect joystick 0
	state.ClearFrameState();
	events.RemoveAll();

	Event disconnectEvent;
	disconnectEvent.type = Event::EType::kJoystickDisconnected;
	disconnectEvent.joystickConnect.joystickId = 0;
	events.Add(disconnectEvent);

	state.ProcessEvents(events);

	EXPECT_FALSE(state.IsJoystickConnected(0));
}

TEST(InputState, JoystickButtonState)
{
	InputState state;
	EventData events;

	// Connect joystick
	Event connectEvent;
	connectEvent.type = Event::EType::kJoystickConnected;
	connectEvent.joystickConnect.joystickId = 0;
	events.Add(connectEvent);

	// Press button 0
	Event pressEvent;
	pressEvent.type = Event::EType::kJoystickButtonPressed;
	pressEvent.joystickButton.joystickId = 0;
	pressEvent.joystickButton.button = 0;
	events.Add(pressEvent);

	state.ProcessEvents(events);

	EXPECT_TRUE(state.IsJoystickButtonDown(0, 0));
	EXPECT_FALSE(state.IsJoystickButtonDown(0, 1));

	// Release button 0
	state.ClearFrameState();
	events.RemoveAll();

	Event releaseEvent;
	releaseEvent.type = Event::EType::kJoystickButtonReleased;
	releaseEvent.joystickButton.joystickId = 0;
	releaseEvent.joystickButton.button = 0;
	events.Add(releaseEvent);

	state.ProcessEvents(events);

	EXPECT_FALSE(state.IsJoystickButtonDown(0, 0));
}

TEST(InputState, JoystickAxisValues)
{
	InputState state;
	EventData events;

	// Connect joystick
	Event connectEvent;
	connectEvent.type = Event::EType::kJoystickConnected;
	connectEvent.joystickConnect.joystickId = 0;
	events.Add(connectEvent);

	// Move X axis
	Event axisEvent;
	axisEvent.type = Event::EType::kJoystickMoved;
	axisEvent.joystickMove.joystickId = 0;
	axisEvent.joystickMove.axis = static_cast<int>(EJoystickAxis::X);
	axisEvent.joystickMove.position = 75.0f;  // SFML uses -100 to 100
	events.Add(axisEvent);

	state.ProcessEvents(events);

	float value = state.GetJoystickAxisValue(0, EJoystickAxis::X);
	EXPECT_FLOAT_EQ(value, 75.0f);
}

TEST(InputState, JoystickPosition)
{
	InputState state;
	EventData events;

	// Connect joystick
	Event connectEvent;
	connectEvent.type = Event::EType::kJoystickConnected;
	connectEvent.joystickConnect.joystickId = 0;
	events.Add(connectEvent);

	// Move X axis
	Event xAxisEvent;
	xAxisEvent.type = Event::EType::kJoystickMoved;
	xAxisEvent.joystickMove.joystickId = 0;
	xAxisEvent.joystickMove.axis = static_cast<int>(EJoystickAxis::X);
	xAxisEvent.joystickMove.position = 50.0f;
	events.Add(xAxisEvent);

	// Move Y axis
	Event yAxisEvent;
	yAxisEvent.type = Event::EType::kJoystickMoved;
	yAxisEvent.joystickMove.joystickId = 0;
	yAxisEvent.joystickMove.axis = static_cast<int>(EJoystickAxis::Y);
	yAxisEvent.joystickMove.position = -25.0f;
	events.Add(yAxisEvent);

	state.ProcessEvents(events);

	float x, y;
	state.GetJoystickPosition(0, x, y);

	EXPECT_FLOAT_EQ(x, 50.0f);
	EXPECT_FLOAT_EQ(y, -25.0f);
}

////////////////////////////////////////////////////////////////////////////////
// Edge Cases and Mixed Input Tests
////////////////////////////////////////////////////////////////////////////////

TEST(InputState, ClearFrameStateResetsTransientState)
{
	InputState state;
	EventData events;

	// Press key
	Event pressEvent;
	pressEvent.type = Event::EType::kKeyPressed;
	pressEvent.key = static_cast<int>(EKey::W);
	events.Add(pressEvent);
	state.ProcessEvents(events);

	EXPECT_TRUE(state.WasKeyPressed(EKey::W));

	// Clear frame state
	state.ClearFrameState();

	EXPECT_TRUE(state.IsKeyDown(EKey::W));  // Still held
	EXPECT_FALSE(state.WasKeyPressed(EKey::W));  // No longer "this frame"
}

TEST(InputState, MultipleInputDevicesSimultaneously)
{
	InputState state;
	EventData events;

	// Press key
	Event keyEvent;
	keyEvent.type = Event::EType::kKeyPressed;
	keyEvent.key = static_cast<int>(EKey::Space);
	events.Add(keyEvent);

	// Press mouse button
	Event mouseEvent;
	mouseEvent.type = Event::EType::kMouseButtonPressed;
	mouseEvent.mouseButton = static_cast<int>(EMouseButton::Left);
	events.Add(mouseEvent);

	// Connect gamepad and press A
	Event connectEvent;
	connectEvent.type = Event::EType::kConsoleGamepadConnected;
	connectEvent.consoleGamepadConnectionEvent.gamepadIndex = 0;
	events.Add(connectEvent);

	Event gamepadEvent;
	gamepadEvent.type = Event::EType::kConsoleGamepadButtonPressed;
	gamepadEvent.consoleGamepadButtonEvent.gamepadIndex = 0;
	gamepadEvent.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events.Add(gamepadEvent);

	// Connect joystick and press button 0
	Event joystickConnectEvent;
	joystickConnectEvent.type = Event::EType::kJoystickConnected;
	joystickConnectEvent.joystickConnect.joystickId = 0;
	events.Add(joystickConnectEvent);

	Event joystickEvent;
	joystickEvent.type = Event::EType::kJoystickButtonPressed;
	joystickEvent.joystickButton.joystickId = 0;
	joystickEvent.joystickButton.button = 0;
	events.Add(joystickEvent);

	state.ProcessEvents(events);

	// All inputs should be active
	EXPECT_TRUE(state.IsKeyDown(EKey::Space));
	EXPECT_TRUE(state.IsMouseButtonDown(EMouseButton::Left));
	EXPECT_TRUE(state.IsGamepadConnected(0));
	EXPECT_TRUE(state.IsGamepadButtonDown(0, ConsoleGamepad::EButtonID::A));
	EXPECT_TRUE(state.IsJoystickConnected(0));
	EXPECT_TRUE(state.IsJoystickButtonDown(0, 0));
}
