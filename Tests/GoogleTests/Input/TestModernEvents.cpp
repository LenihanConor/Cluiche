////////////////////////////////////////////////////////////////////////////////
// Filename: TestModernEvents.cpp - Google Test for Modern Event Classes
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/Events/KeyboardEvents.h>
#include <DiaInput/Events/MouseEvents.h>
#include <DiaInput/Events/GamepadEvents.h>
#include <DiaInput/Events/JoystickEvents.h>
#include <DiaCore/Architecture/Events/EventDispatcher.h>

using namespace Dia::Input::Events;
using namespace Dia::Core::Events;

////////////////////////////////////////////////////////////////////////////////
// Keyboard Event Tests
////////////////////////////////////////////////////////////////////////////////

TEST(KeyboardEvents, KeyPressedEvent)
{
	KeyPressedEvent evt(EKey::W, false, true, false, false);

	EXPECT_EQ(evt.GetEventType(), KeyPressedEvent::GetStaticType());
	EXPECT_EQ(evt.GetKey(), EKey::W);
	EXPECT_FALSE(evt.IsAlt());
	EXPECT_TRUE(evt.IsControl());
	EXPECT_FALSE(evt.IsShift());
	EXPECT_FALSE(evt.IsSystem());
}

TEST(KeyboardEvents, KeyReleasedEvent)
{
	KeyReleasedEvent evt(EKey::Space, false, false, true, false);

	EXPECT_EQ(evt.GetEventType(), KeyReleasedEvent::GetStaticType());
	EXPECT_EQ(evt.GetKey(), EKey::Space);
	EXPECT_FALSE(evt.IsAlt());
	EXPECT_FALSE(evt.IsControl());
	EXPECT_TRUE(evt.IsShift());
	EXPECT_FALSE(evt.IsSystem());
}

TEST(KeyboardEvents, TextEnteredEvent)
{
	TextEnteredEvent evt(65);  // 'A'

	EXPECT_EQ(evt.GetEventType(), TextEnteredEvent::GetStaticType());
	EXPECT_EQ(evt.GetUnicode(), 65u);
}

TEST(KeyboardEvents, DispatchKeyPressedEvent)
{
	EventDispatcher dispatcher;

	bool received = false;
	EKey receivedKey = EKey::Unknown;

	dispatcher.Subscribe<KeyPressedEvent>([&](KeyPressedEvent* evt) {
		received = true;
		receivedKey = evt->GetKey();
	});

	KeyPressedEvent* evt = new KeyPressedEvent(EKey::Enter, false, false, false, false);
	dispatcher.Dispatch(evt);

	EXPECT_TRUE(received);
	EXPECT_EQ(receivedKey, EKey::Enter);
}

////////////////////////////////////////////////////////////////////////////////
// Mouse Event Tests
////////////////////////////////////////////////////////////////////////////////

TEST(MouseEvents, MouseButtonPressedEvent)
{
	MouseButtonPressedEvent evt(EMouseButton::Left, 150, 250);

	EXPECT_EQ(evt.GetEventType(), MouseButtonPressedEvent::GetStaticType());
	EXPECT_EQ(evt.GetButton(), EMouseButton::Left);
	EXPECT_EQ(evt.GetX(), 150);
	EXPECT_EQ(evt.GetY(), 250);
}

TEST(MouseEvents, MouseButtonReleasedEvent)
{
	MouseButtonReleasedEvent evt(EMouseButton::Right, 300, 400);

	EXPECT_EQ(evt.GetEventType(), MouseButtonReleasedEvent::GetStaticType());
	EXPECT_EQ(evt.GetButton(), EMouseButton::Right);
	EXPECT_EQ(evt.GetX(), 300);
	EXPECT_EQ(evt.GetY(), 400);
}

TEST(MouseEvents, MouseMovedEvent)
{
	MouseMovedEvent evt(500, 600);

	EXPECT_EQ(evt.GetEventType(), MouseMovedEvent::GetStaticType());
	EXPECT_EQ(evt.GetX(), 500);
	EXPECT_EQ(evt.GetY(), 600);
}

TEST(MouseEvents, MouseWheelMovedEvent)
{
	MouseWheelMovedEvent evt(3, 100, 200);

	EXPECT_EQ(evt.GetEventType(), MouseWheelMovedEvent::GetStaticType());
	EXPECT_EQ(evt.GetDelta(), 3);
	EXPECT_EQ(evt.GetX(), 100);
	EXPECT_EQ(evt.GetY(), 200);
}

TEST(MouseEvents, MouseEnteredEvent)
{
	MouseEnteredEvent evt;

	EXPECT_EQ(evt.GetEventType(), MouseEnteredEvent::GetStaticType());
}

TEST(MouseEvents, MouseLeftEvent)
{
	MouseLeftEvent evt;

	EXPECT_EQ(evt.GetEventType(), MouseLeftEvent::GetStaticType());
}

TEST(MouseEvents, DispatchMouseEvents)
{
	EventDispatcher dispatcher;

	int mouseMovedCount = 0;
	int mouseClickedCount = 0;

	dispatcher.Subscribe<MouseMovedEvent>([&](MouseMovedEvent* evt) {
		mouseMovedCount++;
	});

	dispatcher.Subscribe<MouseButtonPressedEvent>([&](MouseButtonPressedEvent* evt) {
		mouseClickedCount++;
	});

	// Dispatch multiple events
	dispatcher.Dispatch(new MouseMovedEvent(100, 100));
	dispatcher.Dispatch(new MouseMovedEvent(150, 150));
	dispatcher.Dispatch(new MouseButtonPressedEvent(EMouseButton::Left, 150, 150));

	EXPECT_EQ(mouseMovedCount, 2);
	EXPECT_EQ(mouseClickedCount, 1);
}

////////////////////////////////////////////////////////////////////////////////
// Gamepad Event Tests
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadEvents, GamepadButtonPressedEvent)
{
	GamepadButtonPressedEvent evt(0, ConsoleGamepad::EButtonID::A);

	EXPECT_EQ(evt.GetEventType(), GamepadButtonPressedEvent::GetStaticType());
	EXPECT_EQ(evt.GetGamepadIndex(), 0u);
	EXPECT_EQ(evt.GetButton(), ConsoleGamepad::EButtonID::A);
}

TEST(GamepadEvents, GamepadButtonReleasedEvent)
{
	GamepadButtonReleasedEvent evt(1, ConsoleGamepad::EButtonID::B);

	EXPECT_EQ(evt.GetEventType(), GamepadButtonReleasedEvent::GetStaticType());
	EXPECT_EQ(evt.GetGamepadIndex(), 1u);
	EXPECT_EQ(evt.GetButton(), ConsoleGamepad::EButtonID::B);
}

TEST(GamepadEvents, GamepadAnalogStickMoveEvent)
{
	GamepadAnalogStickMoveEvent evt(0, ConsoleGamepad::EButtonID::AnalogStickLeft, 0.5f, -0.75f);

	EXPECT_EQ(evt.GetEventType(), GamepadAnalogStickMoveEvent::GetStaticType());
	EXPECT_EQ(evt.GetGamepadIndex(), 0u);
	EXPECT_EQ(evt.GetStick(), ConsoleGamepad::EButtonID::AnalogStickLeft);
	EXPECT_FLOAT_EQ(evt.GetX(), 0.5f);
	EXPECT_FLOAT_EQ(evt.GetY(), -0.75f);
}

TEST(GamepadEvents, GamepadTriggerEvent)
{
	GamepadTriggerEvent evt(0, ConsoleGamepad::EButtonID::TriggerLeft, 0.8f);

	EXPECT_EQ(evt.GetEventType(), GamepadTriggerEvent::GetStaticType());
	EXPECT_EQ(evt.GetGamepadIndex(), 0u);
	EXPECT_EQ(evt.GetTrigger(), ConsoleGamepad::EButtonID::TriggerLeft);
	EXPECT_FLOAT_EQ(evt.GetValue(), 0.8f);
}

TEST(GamepadEvents, GamepadConnectedEvent)
{
	GamepadConnectedEvent evt(2);

	EXPECT_EQ(evt.GetEventType(), GamepadConnectedEvent::GetStaticType());
	EXPECT_EQ(evt.GetGamepadIndex(), 2u);
}

TEST(GamepadEvents, GamepadDisconnectedEvent)
{
	GamepadDisconnectedEvent evt(3);

	EXPECT_EQ(evt.GetEventType(), GamepadDisconnectedEvent::GetStaticType());
	EXPECT_EQ(evt.GetGamepadIndex(), 3u);
}

TEST(GamepadEvents, DispatchGamepadEvents)
{
	EventDispatcher dispatcher;

	bool buttonPressed = false;
	bool stickMoved = false;
	bool connected = false;

	dispatcher.Subscribe<GamepadButtonPressedEvent>([&](GamepadButtonPressedEvent* evt) {
		buttonPressed = true;
	});

	dispatcher.Subscribe<GamepadAnalogStickMoveEvent>([&](GamepadAnalogStickMoveEvent* evt) {
		stickMoved = true;
	});

	dispatcher.Subscribe<GamepadConnectedEvent>([&](GamepadConnectedEvent* evt) {
		connected = true;
	});

	dispatcher.Dispatch(new GamepadConnectedEvent(0));
	dispatcher.Dispatch(new GamepadButtonPressedEvent(0, ConsoleGamepad::EButtonID::A));
	dispatcher.Dispatch(new GamepadAnalogStickMoveEvent(0, ConsoleGamepad::EButtonID::AnalogStickLeft, 1.0f, 0.0f));

	EXPECT_TRUE(buttonPressed);
	EXPECT_TRUE(stickMoved);
	EXPECT_TRUE(connected);
}

////////////////////////////////////////////////////////////////////////////////
// Joystick Event Tests
////////////////////////////////////////////////////////////////////////////////

TEST(JoystickEvents, JoystickButtonPressedEvent)
{
	JoystickButtonPressedEvent evt(0, 5);

	EXPECT_EQ(evt.GetEventType(), JoystickButtonPressedEvent::GetStaticType());
	EXPECT_EQ(evt.GetJoystickId(), 0u);
	EXPECT_EQ(evt.GetButton(), 5u);
}

TEST(JoystickEvents, JoystickButtonReleasedEvent)
{
	JoystickButtonReleasedEvent evt(1, 3);

	EXPECT_EQ(evt.GetEventType(), JoystickButtonReleasedEvent::GetStaticType());
	EXPECT_EQ(evt.GetJoystickId(), 1u);
	EXPECT_EQ(evt.GetButton(), 3u);
}

TEST(JoystickEvents, JoystickAxisMovedEvent)
{
	JoystickAxisMovedEvent evt(0, EJoystickAxis::X, 75.0f);

	EXPECT_EQ(evt.GetEventType(), JoystickAxisMovedEvent::GetStaticType());
	EXPECT_EQ(evt.GetJoystickId(), 0u);
	EXPECT_EQ(evt.GetAxis(), EJoystickAxis::X);
	EXPECT_FLOAT_EQ(evt.GetPosition(), 75.0f);
}

TEST(JoystickEvents, JoystickConnectedEvent)
{
	JoystickConnectedEvent evt(2);

	EXPECT_EQ(evt.GetEventType(), JoystickConnectedEvent::GetStaticType());
	EXPECT_EQ(evt.GetJoystickId(), 2u);
}

TEST(JoystickEvents, JoystickDisconnectedEvent)
{
	JoystickDisconnectedEvent evt(3);

	EXPECT_EQ(evt.GetEventType(), JoystickDisconnectedEvent::GetStaticType());
	EXPECT_EQ(evt.GetJoystickId(), 3u);
}

TEST(JoystickEvents, AllJoystickAxes)
{
	// Test all axis types
	JoystickAxisMovedEvent xEvt(0, EJoystickAxis::X, 10.0f);
	EXPECT_EQ(xEvt.GetAxis(), EJoystickAxis::X);

	JoystickAxisMovedEvent yEvt(0, EJoystickAxis::Y, 20.0f);
	EXPECT_EQ(yEvt.GetAxis(), EJoystickAxis::Y);

	JoystickAxisMovedEvent zEvt(0, EJoystickAxis::Z, 30.0f);
	EXPECT_EQ(zEvt.GetAxis(), EJoystickAxis::Z);

	JoystickAxisMovedEvent rEvt(0, EJoystickAxis::R, 40.0f);
	EXPECT_EQ(rEvt.GetAxis(), EJoystickAxis::R);

	JoystickAxisMovedEvent uEvt(0, EJoystickAxis::U, 50.0f);
	EXPECT_EQ(uEvt.GetAxis(), EJoystickAxis::U);

	JoystickAxisMovedEvent vEvt(0, EJoystickAxis::V, 60.0f);
	EXPECT_EQ(vEvt.GetAxis(), EJoystickAxis::V);

	JoystickAxisMovedEvent povXEvt(0, EJoystickAxis::PovX, 70.0f);
	EXPECT_EQ(povXEvt.GetAxis(), EJoystickAxis::PovX);

	JoystickAxisMovedEvent povYEvt(0, EJoystickAxis::PovY, 80.0f);
	EXPECT_EQ(povYEvt.GetAxis(), EJoystickAxis::PovY);
}

TEST(JoystickEvents, DispatchJoystickEvents)
{
	EventDispatcher dispatcher;

	bool buttonPressed = false;
	bool axisMoved = false;
	bool connected = false;

	dispatcher.Subscribe<JoystickButtonPressedEvent>([&](JoystickButtonPressedEvent* evt) {
		buttonPressed = true;
	});

	dispatcher.Subscribe<JoystickAxisMovedEvent>([&](JoystickAxisMovedEvent* evt) {
		axisMoved = true;
	});

	dispatcher.Subscribe<JoystickConnectedEvent>([&](JoystickConnectedEvent* evt) {
		connected = true;
	});

	dispatcher.Dispatch(new JoystickConnectedEvent(0));
	dispatcher.Dispatch(new JoystickButtonPressedEvent(0, 0));
	dispatcher.Dispatch(new JoystickAxisMovedEvent(0, EJoystickAxis::X, 50.0f));

	EXPECT_TRUE(buttonPressed);
	EXPECT_TRUE(axisMoved);
	EXPECT_TRUE(connected);
}

////////////////////////////////////////////////////////////////////////////////
// Event Priority Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ModernEvents, EventDispatcherPriority)
{
	EventDispatcher dispatcher;

	int executionOrder = 0;
	int highPriorityOrder = 0;
	int lowPriorityOrder = 0;

	// Subscribe with high priority
	dispatcher.Subscribe<KeyPressedEvent>([&](KeyPressedEvent* evt) {
		highPriorityOrder = ++executionOrder;
	}, EventPriority::High);

	// Subscribe with low priority
	dispatcher.Subscribe<KeyPressedEvent>([&](KeyPressedEvent* evt) {
		lowPriorityOrder = ++executionOrder;
	}, EventPriority::Low);

	dispatcher.Dispatch(new KeyPressedEvent(EKey::Space, false, false, false, false));

	// High priority should execute before low priority
	EXPECT_EQ(highPriorityOrder, 1);
	EXPECT_EQ(lowPriorityOrder, 2);
}

////////////////////////////////////////////////////////////////////////////////
// Multiple Subscriber Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ModernEvents, MultipleSubscribers)
{
	EventDispatcher dispatcher;

	int subscriber1Count = 0;
	int subscriber2Count = 0;
	int subscriber3Count = 0;

	dispatcher.Subscribe<MouseMovedEvent>([&](MouseMovedEvent* evt) {
		subscriber1Count++;
	});

	dispatcher.Subscribe<MouseMovedEvent>([&](MouseMovedEvent* evt) {
		subscriber2Count++;
	});

	dispatcher.Subscribe<MouseMovedEvent>([&](MouseMovedEvent* evt) {
		subscriber3Count++;
	});

	dispatcher.Dispatch(new MouseMovedEvent(100, 100));

	EXPECT_EQ(subscriber1Count, 1);
	EXPECT_EQ(subscriber2Count, 1);
	EXPECT_EQ(subscriber3Count, 1);
}

////////////////////////////////////////////////////////////////////////////////
// Unsubscribe Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ModernEvents, Unsubscribe)
{
	EventDispatcher dispatcher;

	int callCount = 0;

	auto subscriptionId = dispatcher.Subscribe<KeyPressedEvent>([&](KeyPressedEvent* evt) {
		callCount++;
	});

	dispatcher.Dispatch(new KeyPressedEvent(EKey::Space, false, false, false, false));
	EXPECT_EQ(callCount, 1);

	dispatcher.Unsubscribe<KeyPressedEvent>(subscriptionId);

	dispatcher.Dispatch(new KeyPressedEvent(EKey::Space, false, false, false, false));
	EXPECT_EQ(callCount, 1);  // Should not increment
}

////////////////////////////////////////////////////////////////////////////////
// Mixed Input Type Dispatch Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ModernEvents, DispatchMixedInputTypes)
{
	EventDispatcher dispatcher;

	bool keyboardReceived = false;
	bool mouseReceived = false;
	bool gamepadReceived = false;
	bool joystickReceived = false;

	dispatcher.Subscribe<KeyPressedEvent>([&](KeyPressedEvent* evt) {
		keyboardReceived = true;
	});

	dispatcher.Subscribe<MouseButtonPressedEvent>([&](MouseButtonPressedEvent* evt) {
		mouseReceived = true;
	});

	dispatcher.Subscribe<GamepadButtonPressedEvent>([&](GamepadButtonPressedEvent* evt) {
		gamepadReceived = true;
	});

	dispatcher.Subscribe<JoystickButtonPressedEvent>([&](JoystickButtonPressedEvent* evt) {
		joystickReceived = true;
	});

	// Dispatch events from all input types
	dispatcher.Dispatch(new KeyPressedEvent(EKey::W, false, false, false, false));
	dispatcher.Dispatch(new MouseButtonPressedEvent(EMouseButton::Left, 100, 100));
	dispatcher.Dispatch(new GamepadButtonPressedEvent(0, ConsoleGamepad::EButtonID::A));
	dispatcher.Dispatch(new JoystickButtonPressedEvent(0, 0));

	EXPECT_TRUE(keyboardReceived);
	EXPECT_TRUE(mouseReceived);
	EXPECT_TRUE(gamepadReceived);
	EXPECT_TRUE(joystickReceived);
}

////////////////////////////////////////////////////////////////////////////////
// Event Data Preservation Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ModernEvents, EventDataPreservedThroughDispatch)
{
	EventDispatcher dispatcher;

	EKey receivedKey = EKey::Unknown;
	EMouseButton receivedButton = EMouseButton::Left;
	unsigned int receivedGamepadIndex = 999;
	ConsoleGamepad::EButtonID receivedGamepadButton = ConsoleGamepad::EButtonID::Unknown;

	dispatcher.Subscribe<KeyPressedEvent>([&](KeyPressedEvent* evt) {
		receivedKey = evt->GetKey();
	});

	dispatcher.Subscribe<MouseButtonPressedEvent>([&](MouseButtonPressedEvent* evt) {
		receivedButton = evt->GetButton();
	});

	dispatcher.Subscribe<GamepadButtonPressedEvent>([&](GamepadButtonPressedEvent* evt) {
		receivedGamepadIndex = evt->GetGamepadIndex();
		receivedGamepadButton = evt->GetButton();
	});

	dispatcher.Dispatch(new KeyPressedEvent(EKey::Enter, false, false, false, false));
	dispatcher.Dispatch(new MouseButtonPressedEvent(EMouseButton::Middle, 50, 50));
	dispatcher.Dispatch(new GamepadButtonPressedEvent(2, ConsoleGamepad::EButtonID::X));

	EXPECT_EQ(receivedKey, EKey::Enter);
	EXPECT_EQ(receivedButton, EMouseButton::Middle);
	EXPECT_EQ(receivedGamepadIndex, 2u);
	EXPECT_EQ(receivedGamepadButton, ConsoleGamepad::EButtonID::X);
}
