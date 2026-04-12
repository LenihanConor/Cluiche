#ifndef DIA_INPUT_GAMEPAD_EVENTS_H
#define DIA_INPUT_GAMEPAD_EVENTS_H

#include <DiaCore/Architecture/Events/Event.h>
#include "DiaInput/ConsoleGamepad.h"

namespace Dia
{
	namespace Input
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// GamepadButtonPressedEvent
			//
			// Fired when a gamepad button is pressed.
			//---------------------------------------------------------------------------------------------------------------------------------
			class GamepadButtonPressedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(GamepadButtonPressedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				GamepadButtonPressedEvent(unsigned int gamepadIndex, ConsoleGamepad::EButtonID button)
					: mGamepadIndex(gamepadIndex), mButton(button) {}

				unsigned int GetGamepadIndex() const { return mGamepadIndex; }
				ConsoleGamepad::EButtonID GetButton() const { return mButton; }

			private:
				unsigned int mGamepadIndex;
				ConsoleGamepad::EButtonID mButton;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// GamepadButtonReleasedEvent
			//
			// Fired when a gamepad button is released.
			//---------------------------------------------------------------------------------------------------------------------------------
			class GamepadButtonReleasedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(GamepadButtonReleasedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				GamepadButtonReleasedEvent(unsigned int gamepadIndex, ConsoleGamepad::EButtonID button)
					: mGamepadIndex(gamepadIndex), mButton(button) {}

				unsigned int GetGamepadIndex() const { return mGamepadIndex; }
				ConsoleGamepad::EButtonID GetButton() const { return mButton; }

			private:
				unsigned int mGamepadIndex;
				ConsoleGamepad::EButtonID mButton;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// GamepadAnalogStickMoveEvent
			//
			// Fired when a gamepad analog stick moves outside deadzone.
			//---------------------------------------------------------------------------------------------------------------------------------
			class GamepadAnalogStickMoveEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(GamepadAnalogStickMoveEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				GamepadAnalogStickMoveEvent(unsigned int gamepadIndex, ConsoleGamepad::EButtonID stick, float x, float y)
					: mGamepadIndex(gamepadIndex), mStick(stick), mX(x), mY(y) {}

				unsigned int GetGamepadIndex() const { return mGamepadIndex; }
				ConsoleGamepad::EButtonID GetStick() const { return mStick; }
				float GetX() const { return mX; }
				float GetY() const { return mY; }

			private:
				unsigned int mGamepadIndex;
				ConsoleGamepad::EButtonID mStick;
				float mX;
				float mY;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// GamepadTriggerEvent
			//
			// Fired when a gamepad trigger is pressed beyond threshold.
			//---------------------------------------------------------------------------------------------------------------------------------
			class GamepadTriggerEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(GamepadTriggerEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				GamepadTriggerEvent(unsigned int gamepadIndex, ConsoleGamepad::EButtonID trigger, float value)
					: mGamepadIndex(gamepadIndex), mTrigger(trigger), mValue(value) {}

				unsigned int GetGamepadIndex() const { return mGamepadIndex; }
				ConsoleGamepad::EButtonID GetTrigger() const { return mTrigger; }
				float GetValue() const { return mValue; }

			private:
				unsigned int mGamepadIndex;
				ConsoleGamepad::EButtonID mTrigger;
				float mValue;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// GamepadConnectedEvent
			//
			// Fired when a gamepad is connected (hot-plugged).
			//---------------------------------------------------------------------------------------------------------------------------------
			class GamepadConnectedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(GamepadConnectedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				GamepadConnectedEvent(unsigned int gamepadIndex)
					: mGamepadIndex(gamepadIndex) {}

				unsigned int GetGamepadIndex() const { return mGamepadIndex; }

			private:
				unsigned int mGamepadIndex;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// GamepadDisconnectedEvent
			//
			// Fired when a gamepad is disconnected (unplugged).
			//---------------------------------------------------------------------------------------------------------------------------------
			class GamepadDisconnectedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(GamepadDisconnectedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				GamepadDisconnectedEvent(unsigned int gamepadIndex)
					: mGamepadIndex(gamepadIndex) {}

				unsigned int GetGamepadIndex() const { return mGamepadIndex; }

			private:
				unsigned int mGamepadIndex;
			};
		}
	}
}

#endif // DIA_INPUT_GAMEPAD_EVENTS_H
