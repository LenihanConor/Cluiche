////////////////////////////////////////////////////////////////////////////////
// Filename: TestInputProfile.cpp - Google Test for InputProfile
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/InputProfile.h>
#include <DiaInput/ActionMap.h>
#include <DiaInput/EKey.h>
#include <DiaInput/EMouseButton.h>
#include <DiaInput/ConsoleGamepad.h>
#include <DiaCore/FilePath/FilePath.h>
#include <cstdio>

using namespace Dia::Input;

// Test fixture for file-based tests
class InputProfileTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Create temp directory if it doesn't exist
		Dia::Core::FilePath::MakeDirectory("temp");
	}

	void TearDown() override
	{
		// Clean up test files
		std::remove("temp/test_profile.json");
		std::remove("temp/test_multi_binding.json");
		std::remove("temp/test_all_types.json");
	}
};

////////////////////////////////////////////////////////////////////////////////
// Basic Save/Load Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, SaveAndLoadSingleBinding)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	// Bind Space to Jump
	actionMap.BindKey(jumpAction, EKey::Space);

	// Save profile
	bool saved = InputProfile::SaveProfile(actionMap, "temp/test_profile.json", "TestProfile");
	EXPECT_TRUE(saved);

	// Load profile into new ActionMap
	ActionMap loadedMap;
	bool loaded = InputProfile::LoadProfile(loadedMap, "temp/test_profile.json");
	EXPECT_TRUE(loaded);

	// Verify binding works
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	loadedMap.ProcessEvents(events);
	EXPECT_TRUE(loadedMap.IsActionActive(jumpAction));
}

TEST_F(InputProfileTest, SaveAndLoadMultipleBindings)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");
	ActionID fireAction("Fire");
	ActionID moveAction("Move");

	// Bind multiple actions
	actionMap.BindKey(jumpAction, EKey::Space);
	actionMap.BindMouseButton(fireAction, EMouseButton::Left);
	actionMap.BindKey(moveAction, EKey::W);

	// Save profile
	bool saved = InputProfile::SaveProfile(actionMap, "temp/test_multi_binding.json", "MultiBinding");
	EXPECT_TRUE(saved);

	// Load profile
	ActionMap loadedMap;
	bool loaded = InputProfile::LoadProfile(loadedMap, "temp/test_multi_binding.json");
	EXPECT_TRUE(loaded);

	// Verify all bindings
	EventData events;

	Event spaceEvt;
	spaceEvt.type = Event::EType::kKeyPressed;
	spaceEvt.key = static_cast<int>(EKey::Space);
	events.Add(spaceEvt);

	Event mouseEvt;
	mouseEvt.type = Event::EType::kMouseButtonPressed;
	mouseEvt.mouseButton = static_cast<int>(EMouseButton::Left);
	events.Add(mouseEvt);

	Event wEvt;
	wEvt.type = Event::EType::kKeyPressed;
	wEvt.key = static_cast<int>(EKey::W);
	events.Add(wEvt);

	loadedMap.ProcessEvents(events);

	EXPECT_TRUE(loadedMap.IsActionActive(jumpAction));
	EXPECT_TRUE(loadedMap.IsActionActive(fireAction));
	EXPECT_TRUE(loadedMap.IsActionActive(moveAction));
}

////////////////////////////////////////////////////////////////////////////////
// All Binding Types Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, SaveAndLoadAllBindingTypes)
{
	ActionMap actionMap;

	// Bind all types
	actionMap.BindKey(ActionID("KeyAction"), EKey::Space);
	actionMap.BindMouseButton(ActionID("MouseAction"), EMouseButton::Right);
	actionMap.BindGamepadButton(ActionID("GamepadAction"), 0, ConsoleGamepad::EButtonID::A);
	actionMap.BindJoystickButton(ActionID("JoystickButtonAction"), 0, 0);
	actionMap.BindJoystickAxis(ActionID("JoystickAxisAction"), 0, EJoystickAxis::X);

	// Save profile
	bool saved = InputProfile::SaveProfile(actionMap, "temp/test_all_types.json", "AllTypes");
	EXPECT_TRUE(saved);

	// Load profile
	ActionMap loadedMap;
	bool loaded = InputProfile::LoadProfile(loadedMap, "temp/test_all_types.json");
	EXPECT_TRUE(loaded);

	// Verify key binding
	EventData events1;
	Event keyEvt;
	keyEvt.type = Event::EType::kKeyPressed;
	keyEvt.key = static_cast<int>(EKey::Space);
	events1.Add(keyEvt);
	loadedMap.ProcessEvents(events1);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("KeyAction")));

	// Verify mouse binding
	EventData events2;
	Event mouseEvt;
	mouseEvt.type = Event::EType::kMouseButtonPressed;
	mouseEvt.mouseButton = static_cast<int>(EMouseButton::Right);
	events2.Add(mouseEvt);
	loadedMap.ProcessEvents(events2);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("MouseAction")));

	// Verify gamepad binding
	EventData events3;
	Event gamepadEvt;
	gamepadEvt.type = Event::EType::kConsoleGamepadButtonPressed;
	gamepadEvt.consoleGamepadButtonEvent.gamepadIndex = 0;
	gamepadEvt.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events3.Add(gamepadEvt);
	loadedMap.ProcessEvents(events3);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("GamepadAction")));

	// Verify joystick button binding
	EventData events4;
	Event joystickButtonEvt;
	joystickButtonEvt.type = Event::EType::kJoystickButtonPressed;
	joystickButtonEvt.joystickButton.joystickId = 0;
	joystickButtonEvt.joystickButton.button = 0;
	events4.Add(joystickButtonEvt);
	loadedMap.ProcessEvents(events4);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("JoystickButtonAction")));

	// Verify joystick axis binding
	EventData events5;
	Event joystickAxisEvt;
	joystickAxisEvt.type = Event::EType::kJoystickMoved;
	joystickAxisEvt.joystickMove.joystickId = 0;
	joystickAxisEvt.joystickMove.axis = static_cast<int>(EJoystickAxis::X);
	joystickAxisEvt.joystickMove.position = 75.0f;
	events5.Add(joystickAxisEvt);
	loadedMap.ProcessEvents(events5);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("JoystickAxisAction")));
}

////////////////////////////////////////////////////////////////////////////////
// Multiple Bindings Per Action Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, SaveAndLoadMultipleBindingsPerAction)
{
	ActionMap actionMap;
	ActionID jumpAction("Jump");

	// Bind multiple inputs to same action
	actionMap.BindKey(jumpAction, EKey::Space);
	actionMap.BindGamepadButton(jumpAction, 0, ConsoleGamepad::EButtonID::A);
	actionMap.BindJoystickButton(jumpAction, 0, 0);

	// Save profile
	bool saved = InputProfile::SaveProfile(actionMap, "temp/test_profile.json", "MultiInput");
	EXPECT_TRUE(saved);

	// Load profile
	ActionMap loadedMap;
	bool loaded = InputProfile::LoadProfile(loadedMap, "temp/test_profile.json");
	EXPECT_TRUE(loaded);

	// Verify Space works
	EventData events1;
	Event spaceEvt;
	spaceEvt.type = Event::EType::kKeyPressed;
	spaceEvt.key = static_cast<int>(EKey::Space);
	events1.Add(spaceEvt);
	loadedMap.ProcessEvents(events1);
	EXPECT_TRUE(loadedMap.IsActionActive(jumpAction));

	// Verify Gamepad A works
	EventData events2;
	Event gamepadEvt;
	gamepadEvt.type = Event::EType::kConsoleGamepadButtonPressed;
	gamepadEvt.consoleGamepadButtonEvent.gamepadIndex = 0;
	gamepadEvt.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events2.Add(gamepadEvt);
	loadedMap.ProcessEvents(events2);
	EXPECT_TRUE(loadedMap.IsActionActive(jumpAction));

	// Verify Joystick button 0 works
	EventData events3;
	Event joystickEvt;
	joystickEvt.type = Event::EType::kJoystickButtonPressed;
	joystickEvt.joystickButton.joystickId = 0;
	joystickEvt.joystickButton.button = 0;
	events3.Add(joystickEvt);
	loadedMap.ProcessEvents(events3);
	EXPECT_TRUE(loadedMap.IsActionActive(jumpAction));
}

////////////////////////////////////////////////////////////////////////////////
// Device Index Preservation Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, DeviceIndicesPreserved)
{
	ActionMap actionMap;

	// Bind to specific device indices
	actionMap.BindGamepadButton(ActionID("Gamepad0"), 0, ConsoleGamepad::EButtonID::A);
	actionMap.BindGamepadButton(ActionID("Gamepad1"), 1, ConsoleGamepad::EButtonID::A);
	actionMap.BindJoystickButton(ActionID("Joystick0"), 0, 0);
	actionMap.BindJoystickButton(ActionID("Joystick1"), 1, 0);

	// Save and load
	InputProfile::SaveProfile(actionMap, "temp/test_profile.json", "DeviceIndices");
	ActionMap loadedMap;
	InputProfile::LoadProfile(loadedMap, "temp/test_profile.json");

	// Verify gamepad 0 binding
	EventData events1;
	Event gamepad0Evt;
	gamepad0Evt.type = Event::EType::kConsoleGamepadButtonPressed;
	gamepad0Evt.consoleGamepadButtonEvent.gamepadIndex = 0;
	gamepad0Evt.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events1.Add(gamepad0Evt);
	loadedMap.ProcessEvents(events1);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("Gamepad0")));
	EXPECT_FALSE(loadedMap.IsActionActive(ActionID("Gamepad1")));

	// Verify gamepad 1 binding
	EventData events2;
	Event gamepad1Evt;
	gamepad1Evt.type = Event::EType::kConsoleGamepadButtonPressed;
	gamepad1Evt.consoleGamepadButtonEvent.gamepadIndex = 1;
	gamepad1Evt.consoleGamepadButtonEvent.button = static_cast<int>(ConsoleGamepad::EButtonID::A);
	events2.Add(gamepad1Evt);
	loadedMap.ProcessEvents(events2);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("Gamepad1")));
}

////////////////////////////////////////////////////////////////////////////////
// Profile Name Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, GetProfileName)
{
	ActionMap actionMap;
	actionMap.BindKey(ActionID("Jump"), EKey::Space);

	// Save with specific profile name
	InputProfile::SaveProfile(actionMap, "temp/test_profile.json", "MyCustomProfile");

	// Retrieve profile name
	const char* profileName = nullptr;
	bool success = InputProfile::GetProfileName("temp/test_profile.json", profileName);

	EXPECT_TRUE(success);
	EXPECT_STREQ(profileName, "MyCustomProfile");
}

TEST_F(InputProfileTest, DefaultProfileName)
{
	ActionMap actionMap;
	actionMap.BindKey(ActionID("Jump"), EKey::Space);

	// Save without profile name (should use "Default")
	InputProfile::SaveProfile(actionMap, "temp/test_profile.json");

	// Retrieve profile name
	const char* profileName = nullptr;
	bool success = InputProfile::GetProfileName("temp/test_profile.json", profileName);

	EXPECT_TRUE(success);
	EXPECT_STREQ(profileName, "Default");
}

////////////////////////////////////////////////////////////////////////////////
// Error Handling Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, LoadFromNonExistentFileReturnsFalse)
{
	ActionMap actionMap;
	bool loaded = InputProfile::LoadProfile(actionMap, "temp/nonexistent_file.json");

	EXPECT_FALSE(loaded);
}

TEST_F(InputProfileTest, GetProfileNameFromNonExistentFileReturnsFalse)
{
	const char* profileName = nullptr;
	bool success = InputProfile::GetProfileName("temp/nonexistent_file.json", profileName);

	EXPECT_FALSE(success);
}

////////////////////////////////////////////////////////////////////////////////
// ActionMap Convenience Methods Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, ActionMapSaveProfileConvenienceMethod)
{
	ActionMap actionMap;
	actionMap.BindKey(ActionID("Jump"), EKey::Space);

	// Use ActionMap's convenience method
	bool saved = actionMap.SaveProfile("temp/test_profile.json", "Convenience");
	EXPECT_TRUE(saved);

	// Verify file exists and is valid
	ActionMap loadedMap;
	bool loaded = InputProfile::LoadProfile(loadedMap, "temp/test_profile.json");
	EXPECT_TRUE(loaded);

	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	loadedMap.ProcessEvents(events);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("Jump")));
}

TEST_F(InputProfileTest, ActionMapLoadProfileConvenienceMethod)
{
	// Create and save profile using static method
	ActionMap actionMap;
	actionMap.BindKey(ActionID("Fire"), EKey::MouseLeft);
	InputProfile::SaveProfile(actionMap, "temp/test_profile.json", "Convenience2");

	// Load using ActionMap's convenience method
	ActionMap loadedMap;
	bool loaded = loadedMap.LoadProfile("temp/test_profile.json");
	EXPECT_TRUE(loaded);

	EventData events;
	Event evt;
	evt.type = Event::EType::kMouseButtonPressed;
	evt.mouseButton = static_cast<int>(EMouseButton::Left);
	events.Add(evt);

	loadedMap.ProcessEvents(events);
	EXPECT_TRUE(loadedMap.IsActionActive(ActionID("Fire")));
}

////////////////////////////////////////////////////////////////////////////////
// Clear Bindings Before Load Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, LoadProfileClearsExistingBindings)
{
	ActionMap actionMap;
	actionMap.BindKey(ActionID("OldAction"), EKey::A);

	// Save a profile with different action
	ActionMap saveMap;
	saveMap.BindKey(ActionID("NewAction"), EKey::B);
	InputProfile::SaveProfile(saveMap, "temp/test_profile.json");

	// Load profile (should clear old bindings)
	InputProfile::LoadProfile(actionMap, "temp/test_profile.json");

	// Old action should not work
	EventData events1;
	Event aEvt;
	aEvt.type = Event::EType::kKeyPressed;
	aEvt.key = static_cast<int>(EKey::A);
	events1.Add(aEvt);
	actionMap.ProcessEvents(events1);
	EXPECT_FALSE(actionMap.IsActionActive(ActionID("OldAction")));

	// New action should work
	EventData events2;
	Event bEvt;
	bEvt.type = Event::EType::kKeyPressed;
	bEvt.key = static_cast<int>(EKey::B);
	events2.Add(bEvt);
	actionMap.ProcessEvents(events2);
	EXPECT_TRUE(actionMap.IsActionActive(ActionID("NewAction")));
}

////////////////////////////////////////////////////////////////////////////////
// Empty Profile Tests
////////////////////////////////////////////////////////////////////////////////

TEST_F(InputProfileTest, SaveAndLoadEmptyProfile)
{
	ActionMap actionMap;  // No bindings

	// Save empty profile
	bool saved = InputProfile::SaveProfile(actionMap, "temp/test_profile.json", "Empty");
	EXPECT_TRUE(saved);

	// Load empty profile
	ActionMap loadedMap;
	bool loaded = InputProfile::LoadProfile(loadedMap, "temp/test_profile.json");
	EXPECT_TRUE(loaded);

	// No actions should be active
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	loadedMap.ProcessEvents(events);
	EXPECT_FALSE(loadedMap.IsActionActive(ActionID("Any")));
}
