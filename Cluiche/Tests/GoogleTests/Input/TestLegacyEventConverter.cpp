////////////////////////////////////////////////////////////////////////////////
// Filename: TestLegacyEventConverter.cpp - Google Test for LegacyEventConverter
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/Events/LegacyEventConverter.h>
#include <DiaInput/Events/KeyboardEvents.h>
#include <DiaInput/Events/MouseEvents.h>
#include <DiaInput/Events/GamepadEvents.h>
#include <DiaInput/Events/JoystickEvents.h>
#include <DiaInput/Event.h>
#include <DiaInput/EKey.h>
#include <DiaInput/EMouseButton.h>
#include <DiaInput/ConsoleGamepad.h>

using namespace Dia::Input;
using namespace Dia::Input::Events;

////////////////////////////////////////////////////////////////////////////////
// Keyboard Event Conversion Tests
////////////////////////////////////////////////////////////////////////////////

TEST(LegacyEventConverter, ConvertKeyPressed)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kKeyPressed;
	legacyEvent.key.code = static_cast<int>(EKey::W); legacyEvent.key.alt = false; legacyEvent.key.control = false; legacyEvent.key.shift = false; legacyEvent.key.system = false;
	legacyEvent.key.alt = false;
	legacyEvent.key.control = false;
	legacyEvent.key.shift = false;
	legacyEvent.key.system = false;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), KeyPressedEvent::GetStaticType());

	KeyPressedEvent* keyEvent = dynamic_cast<KeyPressedEvent*>(modernEvent);
	ASSERT_NE(keyEvent, nullptr);
	EXPECT_EQ(keyEvent->GetKey(), EKey::W);
	EXPECT_FALSE(keyEvent->IsAlt());
	EXPECT_FALSE(keyEvent->IsControl());
	EXPECT_FALSE(keyEvent->IsShift());
	EXPECT_FALSE(keyEvent->IsSystem());

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertKeyReleased)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kKeyReleased;
	legacyEvent.key.code = static_cast<int>(EKey::Space); legacyEvent.key.alt = false; legacyEvent.key.control = false; legacyEvent.key.shift = false; legacyEvent.key.system = false;
	legacyEvent.key.alt = false;
	legacyEvent.key.control = true;
	legacyEvent.key.shift = false;
	legacyEvent.key.system = false;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), KeyReleasedEvent::GetStaticType());

	KeyReleasedEvent* keyEvent = dynamic_cast<KeyReleasedEvent*>(modernEvent);
	ASSERT_NE(keyEvent, nullptr);
	EXPECT_EQ(keyEvent->GetKey(), EKey::Space);
	EXPECT_TRUE(keyEvent->IsControl());

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertTextEntered)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kTextEntered;
	legacyEvent.text.unicode = 65;  // 'A'

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), TextEnteredEvent::GetStaticType());

	TextEnteredEvent* textEvent = dynamic_cast<TextEnteredEvent*>(modernEvent);
	ASSERT_NE(textEvent, nullptr);
	EXPECT_EQ(textEvent->GetUnicode(), 65u);

	delete modernEvent;
}

////////////////////////////////////////////////////////////////////////////////
// Mouse Event Conversion Tests
////////////////////////////////////////////////////////////////////////////////

TEST(LegacyEventConverter, ConvertMouseButtonPressed)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kMouseButtonPressed;
	legacyEvent.mouseButton.button = static_cast<int>(EMouseButton::kLeft); legacyEvent.mouseButton.x = 0; legacyEvent.mouseButton.y = 0;
	legacyEvent.mouseButton.x = 100;
	legacyEvent.mouseButton.y = 200;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), MouseButtonPressedEvent::GetStaticType());

	MouseButtonPressedEvent* mouseEvent = dynamic_cast<MouseButtonPressedEvent*>(modernEvent);
	ASSERT_NE(mouseEvent, nullptr);
	EXPECT_EQ(mouseEvent->GetButton(), EMouseButton::kLeft);
	EXPECT_EQ(mouseEvent->GetX(), 100);
	EXPECT_EQ(mouseEvent->GetY(), 200);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertMouseButtonReleased)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kMouseButtonReleased;
	legacyEvent.mouseButton.button = static_cast<int>(EMouseButton::kRight); legacyEvent.mouseButton.x = 0; legacyEvent.mouseButton.y = 0;
	legacyEvent.mouseButton.x = 50;
	legacyEvent.mouseButton.y = 75;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), MouseButtonReleasedEvent::GetStaticType());

	MouseButtonReleasedEvent* mouseEvent = dynamic_cast<MouseButtonReleasedEvent*>(modernEvent);
	ASSERT_NE(mouseEvent, nullptr);
	EXPECT_EQ(mouseEvent->GetButton(), EMouseButton::kRight);
	EXPECT_EQ(mouseEvent->GetX(), 50);
	EXPECT_EQ(mouseEvent->GetY(), 75);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertMouseMoved)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kMouseMoved;
	legacyEvent.mouseMove.x = 300;
	legacyEvent.mouseMove.y = 400;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), MouseMovedEvent::GetStaticType());

	MouseMovedEvent* mouseEvent = dynamic_cast<MouseMovedEvent*>(modernEvent);
	ASSERT_NE(mouseEvent, nullptr);
	EXPECT_EQ(mouseEvent->GetX(), 300);
	EXPECT_EQ(mouseEvent->GetY(), 400);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertMouseWheelMoved)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kMouseWheelMoved;
	legacyEvent.mouseWheel.delta = 3;
	legacyEvent.mouseWheel.x = 150;
	legacyEvent.mouseWheel.y = 250;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), MouseWheelMovedEvent::GetStaticType());

	MouseWheelMovedEvent* mouseEvent = dynamic_cast<MouseWheelMovedEvent*>(modernEvent);
	ASSERT_NE(mouseEvent, nullptr);
	EXPECT_EQ(mouseEvent->GetDelta(), 3);
	EXPECT_EQ(mouseEvent->GetX(), 150);
	EXPECT_EQ(mouseEvent->GetY(), 250);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertMouseEntered)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kMouseEntered;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), MouseEnteredEvent::GetStaticType());

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertMouseLeft)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kMouseLeft;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), MouseLeftEvent::GetStaticType());

	delete modernEvent;
}

////////////////////////////////////////////////////////////////////////////////
// Gamepad Event Conversion Tests
////////////////////////////////////////////////////////////////////////////////

TEST(LegacyEventConverter, ConvertGamepadButtonPressed)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kConsoleGamepadButtonPressed;
	legacyEvent.consoleGamepadButtonEvent.gamepadIndex = 0;
	legacyEvent.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), GamepadButtonPressedEvent::GetStaticType());

	GamepadButtonPressedEvent* gamepadEvent = dynamic_cast<GamepadButtonPressedEvent*>(modernEvent);
	ASSERT_NE(gamepadEvent, nullptr);
	EXPECT_EQ(gamepadEvent->GetGamepadIndex(), 0u);
	EXPECT_EQ(gamepadEvent->GetButton(), ConsoleGamepad::EButtonID::A);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertGamepadButtonReleased)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kConsoleGamepadButtonReleased;
	legacyEvent.consoleGamepadButtonEvent.gamepadIndex = 1;
	legacyEvent.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::B);

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), GamepadButtonReleasedEvent::GetStaticType());

	GamepadButtonReleasedEvent* gamepadEvent = dynamic_cast<GamepadButtonReleasedEvent*>(modernEvent);
	ASSERT_NE(gamepadEvent, nullptr);
	EXPECT_EQ(gamepadEvent->GetGamepadIndex(), 1u);
	EXPECT_EQ(gamepadEvent->GetButton(), ConsoleGamepad::EButtonID::B);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertGamepadAnalogStickMoved)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kConsoleGamepadAnalogStickMove;
	legacyEvent.consoleGamepadMoveEvent.gamepadIndex = 0;
	legacyEvent.consoleGamepadMoveEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::L_Thumbstick);
	legacyEvent.consoleGamepadMoveEvent.x = 0.5f;
	legacyEvent.consoleGamepadMoveEvent.y = -0.75f;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), GamepadAnalogStickMoveEvent::GetStaticType());

	GamepadAnalogStickMoveEvent* stickEvent = dynamic_cast<GamepadAnalogStickMoveEvent*>(modernEvent);
	ASSERT_NE(stickEvent, nullptr);
	EXPECT_EQ(stickEvent->GetGamepadIndex(), 0u);
	EXPECT_EQ(stickEvent->GetStick(), ConsoleGamepad::EButtonID::L_Thumbstick);
	EXPECT_FLOAT_EQ(stickEvent->GetX(), 0.5f);
	EXPECT_FLOAT_EQ(stickEvent->GetY(), -0.75f);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertGamepadTriggerMoved)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kConsoleGamepadAnalogTriggers;
	legacyEvent.consoleGamepadAnalogueTriggerEvent.gamepadIndex = 0;
	legacyEvent.consoleGamepadAnalogueTriggerEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::L_Trigger);
	legacyEvent.consoleGamepadAnalogueTriggerEvent.value = 0.8f;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), GamepadTriggerEvent::GetStaticType());

	GamepadTriggerEvent* triggerEvent = dynamic_cast<GamepadTriggerEvent*>(modernEvent);
	ASSERT_NE(triggerEvent, nullptr);
	EXPECT_EQ(triggerEvent->GetGamepadIndex(), 0u);
	EXPECT_EQ(triggerEvent->GetTrigger(), ConsoleGamepad::EButtonID::L_Trigger);
	EXPECT_FLOAT_EQ(triggerEvent->GetValue(), 0.8f);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertGamepadConnected)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kConsoleGamepadConnected;
	legacyEvent.consoleGamepadConnectEvent.gamepadIndex = 2;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), GamepadConnectedEvent::GetStaticType());

	GamepadConnectedEvent* connectEvent = dynamic_cast<GamepadConnectedEvent*>(modernEvent);
	ASSERT_NE(connectEvent, nullptr);
	EXPECT_EQ(connectEvent->GetGamepadIndex(), 2u);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertGamepadDisconnected)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kConsoleGamepadDisconnected;
	legacyEvent.consoleGamepadConnectEvent.gamepadIndex = 3;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), GamepadDisconnectedEvent::GetStaticType());

	GamepadDisconnectedEvent* disconnectEvent = dynamic_cast<GamepadDisconnectedEvent*>(modernEvent);
	ASSERT_NE(disconnectEvent, nullptr);
	EXPECT_EQ(disconnectEvent->GetGamepadIndex(), 3u);

	delete modernEvent;
}

////////////////////////////////////////////////////////////////////////////////
// Joystick Event Conversion Tests
////////////////////////////////////////////////////////////////////////////////

TEST(LegacyEventConverter, ConvertJoystickButtonPressed)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kJoystickButtonPressed;
	legacyEvent.joystickButton.joystickId = 0;
	legacyEvent.joystickButton.button = 5;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), JoystickButtonPressedEvent::GetStaticType());

	JoystickButtonPressedEvent* joystickEvent = dynamic_cast<JoystickButtonPressedEvent*>(modernEvent);
	ASSERT_NE(joystickEvent, nullptr);
	EXPECT_EQ(joystickEvent->GetJoystickId(), 0u);
	EXPECT_EQ(joystickEvent->GetButton(), 5u);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertJoystickButtonReleased)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kJoystickButtonReleased;
	legacyEvent.joystickButton.joystickId = 1;
	legacyEvent.joystickButton.button = 3;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), JoystickButtonReleasedEvent::GetStaticType());

	JoystickButtonReleasedEvent* joystickEvent = dynamic_cast<JoystickButtonReleasedEvent*>(modernEvent);
	ASSERT_NE(joystickEvent, nullptr);
	EXPECT_EQ(joystickEvent->GetJoystickId(), 1u);
	EXPECT_EQ(joystickEvent->GetButton(), 3u);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertJoystickMoved)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kJoystickMoved;
	legacyEvent.joystickMove.joystickId = 0;
	legacyEvent.joystickMove.axis = static_cast<int>(EJoystickAxis::X);
	legacyEvent.joystickMove.position = 75.0f;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), JoystickAxisMovedEvent::GetStaticType());

	JoystickAxisMovedEvent* axisEvent = dynamic_cast<JoystickAxisMovedEvent*>(modernEvent);
	ASSERT_NE(axisEvent, nullptr);
	EXPECT_EQ(axisEvent->GetJoystickId(), 0u);
	EXPECT_EQ(axisEvent->GetAxis(), EJoystickAxis::X);
	EXPECT_FLOAT_EQ(axisEvent->GetPosition(), 75.0f);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertJoystickConnected)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kJoystickConnected;
	legacyEvent.joystickConnect.joystickId = 2;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), JoystickConnectedEvent::GetStaticType());

	JoystickConnectedEvent* connectEvent = dynamic_cast<JoystickConnectedEvent*>(modernEvent);
	ASSERT_NE(connectEvent, nullptr);
	EXPECT_EQ(connectEvent->GetJoystickId(), 2u);

	delete modernEvent;
}

TEST(LegacyEventConverter, ConvertJoystickDisconnected)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::kJoystickDisconnected;
	legacyEvent.joystickConnect.joystickId = 3;

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	ASSERT_NE(modernEvent, nullptr);
	EXPECT_EQ(modernEvent->GetEventType(), JoystickDisconnectedEvent::GetStaticType());

	JoystickDisconnectedEvent* disconnectEvent = dynamic_cast<JoystickDisconnectedEvent*>(modernEvent);
	ASSERT_NE(disconnectEvent, nullptr);
	EXPECT_EQ(disconnectEvent->GetJoystickId(), 3u);

	delete modernEvent;
}

////////////////////////////////////////////////////////////////////////////////
// Batch Conversion Tests
////////////////////////////////////////////////////////////////////////////////

TEST(LegacyEventConverter, ConvertAndDispatchMultipleEvents)
{
	// Create EventDispatcher
	Dia::Core::Events::EventDispatcher dispatcher;

	// Track which events were received
	bool keyPressedReceived = false;
	bool mousePressedReceived = false;
	bool gamepadPressedReceived = false;

	// Subscribe to modern events
	dispatcher.Subscribe<KeyPressedEvent>([&](KeyPressedEvent* evt) {
		keyPressedReceived = true;
	});

	dispatcher.Subscribe<MouseButtonPressedEvent>([&](MouseButtonPressedEvent* evt) {
		mousePressedReceived = true;
	});

	dispatcher.Subscribe<GamepadButtonPressedEvent>([&](GamepadButtonPressedEvent* evt) {
		gamepadPressedReceived = true;
	});

	// Create legacy events
	EventData legacyEvents;

	Event keyEvt;
	keyEvt.type = Event::EType::kKeyPressed;
	keyEvt.key.code = static_cast<int>(EKey::Space);
	keyEvt.key.alt = false;
	keyEvt.key.control = false;
	keyEvt.key.shift = false;
	keyEvt.key.system = false;
	legacyEvents.Add(keyEvt);

	Event mouseEvt;
	mouseEvt.type = Event::EType::kMouseButtonPressed;
	mouseEvt.mouseButton.button = static_cast<int>(EMouseButton::kLeft);
	mouseEvt.mouseButton.x = 100;
	mouseEvt.mouseButton.y = 200;
	legacyEvents.Add(mouseEvt);

	Event gamepadEvt;
	gamepadEvt.type = Event::EType::kConsoleGamepadButtonPressed;
	gamepadEvt.consoleGamepadButtonEvent.gamepadIndex = 0;
	gamepadEvt.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	legacyEvents.Add(gamepadEvt);

	// Convert and dispatch
	LegacyEventConverter::ConvertAndDispatch(legacyEvents, dispatcher);

	// Verify all events were dispatched
	EXPECT_TRUE(keyPressedReceived);
	EXPECT_TRUE(mousePressedReceived);
	EXPECT_TRUE(gamepadPressedReceived);
}

////////////////////////////////////////////////////////////////////////////////
// Edge Cases
////////////////////////////////////////////////////////////////////////////////

TEST(LegacyEventConverter, ConvertUnsupportedEventReturnsNull)
{
	Event legacyEvent;
	legacyEvent.type = Event::EType::CreateFromInt(9999);  // Invalid type

	Dia::Core::Events::Event* modernEvent = LegacyEventConverter::ToModernEvent(legacyEvent);

	EXPECT_EQ(modernEvent, nullptr);
}

TEST(LegacyEventConverter, ConvertAndDispatchEmptyEventData)
{
	Dia::Core::Events::EventDispatcher dispatcher;
	EventData emptyEvents;

	// Should not crash
	LegacyEventConverter::ConvertAndDispatch(emptyEvents, dispatcher);
}
