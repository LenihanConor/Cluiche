////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleGamepad.h: 
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <windows.h>
#include <Xinput.h>

#include <DiaCore/Core/EnumClass.h>

namespace Dia
{
	namespace Input
	{
		class ConsoleGamepad
		{
		public:
			// Function prototypes
			//---------------------//
			////////////////////////////////////////////////////////////////////////////////
			// Enum name: EButtonID - Order is very important here
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM(EButtonID, \
				CE_ITEMVAL(A, 0)\
				CE_ITEM(B)\
				CE_ITEM(X)\
				CE_ITEM(Y)\
				CE_ITEM(DPad_Up)\
				CE_ITEM(DPad_Down)\
				CE_ITEM(DPad_Left)\
				CE_ITEM(DPad_Right)\
				CE_ITEM(L_Shoulder)\
				CE_ITEM(R_Shoulder)\
				CE_ITEM(Start)\
				CE_ITEM(Back)\
				CE_ITEM(L_Thumbstick)\
				CE_ITEM(R_Thumbstick)\
				CE_ITEM(L_Trigger)\
				CE_ITEM(R_Trigger)\
				, A \
				);

			static const unsigned int GetMaxDigitalButtons(){ return EButtonID::R_Thumbstick; };

			// Constructors
			ConsoleGamepad();

			void Initialize(int index);
 
			void Update(); // Update gamepad state
			void RefreshState(); // Update button states for next frame

			// Thumbstick functions
			// - Return true if stick is inside deadzone, false if outside
			bool IsLStickInDeadzone()const;
			bool IsRStickInDeadzone()const;

			float GetLeftStickX()const;  // Return X axis of left stick
			float GetLeftStickY()const;  // Return Y axis of left stick
			float GetRightStickX()const; // Return X axis of right stick
			float GetRightStickY()const; // Return Y axis of right stick

			// Trigger functions
			float GetLeftTrigger()const;  // Return value of left trigger
			float GetRightTrigger()const; // Return value of right trigger

			// Button functions
			// - 'Pressed' - Return true if pressed, false if not
			bool IsButtonPressed(int buttonId)const;
			bool IsButtonReleased(int buttonId)const;

			// Utility functions
			XINPUT_STATE GetState()const; // Return gamepad state
			int GetIndex()const;          // Return gamepad index
			bool IsConnected()const;        // Return true if gamepad is connected

			// Vibrate the gamepad (0.0f cancel, 1.0f max speed)
			void Rumble(float leftMotor = 0.0f, float rightMotor = 0.0f);

		private:
			// Member variables
			//---------------------//

			XINPUT_STATE mState; // Current gamepad state
			int mGamepadIndex;  // Gamepad index (eg. 1,2,3,4)

			static const int kButtonCount = 14;    // Total gamepad buttons
			bool mPrevButtonStates[kButtonCount]; // Previous frame button states
			bool mButtonStates[kButtonCount];      // Current frame button states

			// Buttons pressed on current frame
			bool mGamepadButtonsDown[kButtonCount];
		};
	}
}