////////////////////////////////////////////////////////////////////////////////
// Filename: DummyClass
////////////////////////////////////////////////////////////////////////////////
#include "DiaInput/ConsoleGamepad.h"

// Link the 'Xinput' library - Important!
#pragma comment(lib, "Xinput.lib")

// XInput Button values
static const WORD XINPUT_Buttons[] = {
	XINPUT_GAMEPAD_A,
	XINPUT_GAMEPAD_B,
	XINPUT_GAMEPAD_X,
	XINPUT_GAMEPAD_Y,
	XINPUT_GAMEPAD_DPAD_UP,
	XINPUT_GAMEPAD_DPAD_DOWN,
	XINPUT_GAMEPAD_DPAD_LEFT,
	XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_LEFT_SHOULDER,
	XINPUT_GAMEPAD_RIGHT_SHOULDER,
	XINPUT_GAMEPAD_LEFT_THUMB,
	XINPUT_GAMEPAD_RIGHT_THUMB,
	XINPUT_GAMEPAD_START,
	XINPUT_GAMEPAD_BACK
};

namespace Dia
{
	namespace Input
	{
		// Default constructor
		ConsoleGamepad::ConsoleGamepad() {}

		// Overloaded constructor
		void ConsoleGamepad::Initialize(int index)
		{
			// Set gamepad index
			mGamepadIndex = index - 1;

			// Iterate through all gamepad buttons
			for (int i = 0; i < kButtonCount; i++)
			{
				mPrevButtonStates[i] = false;
				mButtonStates[i] = false;
				mGamepadButtonsDown[i] = false;
			}
		}

		// Return gamepad state
		XINPUT_STATE ConsoleGamepad::GetState()const
		{
			// Temporary XINPUT_STATE to return
			XINPUT_STATE GamepadState;

			// Zero memory
			ZeroMemory(&GamepadState, sizeof(XINPUT_STATE));

			// Get the state of the gamepad
			XInputGetState(mGamepadIndex, &GamepadState);

			// Return the gamepad state
			return GamepadState;
		}

		// Return gamepad index
		int ConsoleGamepad::GetIndex()const
		{
			return mGamepadIndex;
		}

		// Return true if the gamepad is connected
		bool ConsoleGamepad::IsConnected()const
		{
			// Zero memory
			ZeroMemory((const_cast<XINPUT_STATE*>(&mState)), sizeof(XINPUT_STATE));

			// Get the state of the gamepad
			DWORD Result = XInputGetState(mGamepadIndex, (const_cast<XINPUT_STATE*>(&mState)));

			if (Result == ERROR_SUCCESS)
			{
				return true;  // The gamepad is connected
			}

			return false; // The gamepad is not connected
		}

		// Update gamepad state
		void ConsoleGamepad::Update()
		{
			mState = GetState(); // Obtain current gamepad state

			// Iterate through all gamepad buttons
			for (int i = 0; i < kButtonCount; i++)
			{
				// Set button state for current frame
				mButtonStates[i] = (mState.Gamepad.wButtons & XINPUT_Buttons[i]) == XINPUT_Buttons[i];

				// Set 'DOWN' state for current frame
				mGamepadButtonsDown[i] = !mPrevButtonStates[i] && mButtonStates[i];
			}
		}

		// Update button states for next frame
		void ConsoleGamepad::RefreshState()
		{
			memcpy(mPrevButtonStates, mButtonStates, sizeof(mPrevButtonStates));
		}

		// Deadzone check for Left Thumbstick
		bool ConsoleGamepad::IsLStickInDeadzone()const
		{
			// Obtain the X & Y axes of the stick
			short sX = mState.Gamepad.sThumbLX;
			short sY = mState.Gamepad.sThumbLY;

			// X axis is outside of deadzone
			if (sX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
				sX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			{
				return false;
			}

			// Y axis is outside of deadzone
			if (sY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
				sY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			{
				return false;
			}
			// One (or both axes) axis is inside of deadzone
			return true;
		}

		// Deadzone check for Right Thumbstick
		bool ConsoleGamepad::IsRStickInDeadzone()const
		{
			// Obtain the X & Y axes of the stick
			short sX = mState.Gamepad.sThumbRX;
			short sY = mState.Gamepad.sThumbRY;

			// X axis is outside of deadzone
			if (sX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
				sX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			{
				return false;
			}
			// Y axis is outside of deadzone
			if (sY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
				sY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			{
				return false;
			}

			// One (or both axes) axis is inside of deadzone
			return true;
		}

		// Return X axis of left stick
		float ConsoleGamepad::GetLeftStickX()const
		{
			// Obtain X axis of left stick
			short sX = mState.Gamepad.sThumbLX;

			// Return axis value, converted to a float
			return (static_cast<float>(sX) / 32768.0f);
		}

		// Return Y axis of left stick
		float ConsoleGamepad::GetLeftStickY()const
		{
			// Obtain Y axis of left stick
			short sY = mState.Gamepad.sThumbLY;

			// Return axis value, converted to a float
			return (static_cast<float>(sY) / 32768.0f);
		}

		// Return X axis of right stick
		float ConsoleGamepad::GetRightStickX()const
		{
			// Obtain X axis of right stick
			short sX = mState.Gamepad.sThumbRX;

			// Return axis value, converted to a float
			return (static_cast<float>(sX) / 32768.0f);
		}

		// Return Y axis of right stick
		float ConsoleGamepad::GetRightStickY()const
		{
			// Obtain the Y axis of the left stick
			short sY = mState.Gamepad.sThumbRY;

			// Return axis value, converted to a float
			return (static_cast<float>(sY) / 32768.0f);
		}

		// Return value of left trigger
		float ConsoleGamepad::GetLeftTrigger()const
		{
			// Obtain value of left trigger
			BYTE Trigger = mState.Gamepad.bLeftTrigger;

			if (Trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				return Trigger / 255.0f;

			return 0.0f; // Trigger was not pressed
		}

		// Return value of right trigger
		float ConsoleGamepad::GetRightTrigger()const
		{
			// Obtain value of right trigger
			BYTE Trigger = mState.Gamepad.bRightTrigger;

			if (Trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			{
				return Trigger / 255.0f;
			}

			return 0.0f; // Trigger was not pressed
		}

		// Vibrate the gamepad (values of 0.0f to 1.0f only)
		void ConsoleGamepad::Rumble(float leftMotor, float rightMotor)
		{
			// Vibration state
			XINPUT_VIBRATION VibrationState;

			// Zero memory
			ZeroMemory(&VibrationState, sizeof(XINPUT_VIBRATION));

			// Calculate vibration values
			int iLeftMotor = int(leftMotor * 65535.0f);
			int iRightMotor = int(rightMotor * 65535.0f);

			// Set vibration values
			VibrationState.wLeftMotorSpeed = iLeftMotor;
			VibrationState.wRightMotorSpeed = iRightMotor;

			// Set the vibration state
			XInputSetState(mGamepadIndex, &VibrationState);
		}

		// Return true if button is pressed, false if not
		bool ConsoleGamepad::IsButtonPressed(int buttonId)const
		{
			if (mState.Gamepad.wButtons & XINPUT_Buttons[buttonId])
			{
				return true; // The button is pressed
			}

			return false; // The button is not pressed
		}

		bool ConsoleGamepad::IsButtonReleased(int buttonId)const
		{
			if (mPrevButtonStates[buttonId] && !IsButtonPressed(buttonId))
			{
				return true;
			}
			return false;
		}
	}
}