////////////////////////////////////////////////////////////////////////////////
// Filename: Event.h: Single input event
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/EKey.h"
#include "DiaInput/EMouseButton.h"
#include "DiaInput/EJoystick.h"
#include "DiaInput/ConsoleGamepad.h"

namespace Dia
{
	namespace Input
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: IPoll
		////////////////////////////////////////////////////////////////////////////////
		class Event
		{
		public:
			////////////////////////////////////////////////////////////
			/// \brief Size events parameters (Resized)
			///
			////////////////////////////////////////////////////////////
			struct SizeEvent
			{
				unsigned int width;  ///< New width, in pixels
				unsigned int height; ///< New height, in pixels
			};

			////////////////////////////////////////////////////////////
			/// \brief Keyboard event parameters (KeyPressed, KeyReleased)
			///
			////////////////////////////////////////////////////////////
			struct KeyEvent
			{
				EKey  AsKey()const { return EKey::CreateFromInt(code); }

				int			  code;    ///< Code of the key that has been pressed
				bool          alt;     ///< Is the Alt key pressed?
				bool          control; ///< Is the Control key pressed?
				bool          shift;   ///< Is the Shift key pressed?
				bool          system;  ///< Is the System key pressed?
			};

			////////////////////////////////////////////////////////////
			/// \brief Text event parameters (TextEntered)
			///
			////////////////////////////////////////////////////////////
			struct TextEvent
			{
				unsigned int unicode; ///< UTF-32 unicode value of the character
			};

			////////////////////////////////////////////////////////////
			/// \brief Mouse move event parameters (MouseMoved)
			///
			////////////////////////////////////////////////////////////
			struct MouseMoveEvent
			{
				int x; ///< X position of the mouse pointer, relative to the left of the owner window
				int y; ///< Y position of the mouse pointer, relative to the top of the owner window
			};

			////////////////////////////////////////////////////////////
			/// \brief Mouse buttons events parameters
			///        (MouseButtonPressed, MouseButtonReleased)
			///
			////////////////////////////////////////////////////////////
			struct MouseButtonEvent
			{
				EMouseButton  AsMouseButton()const { return EMouseButton::CreateFromInt(button); }

				int			  button; ///< Code of the button that has been pressed
				int           x;      ///< X position of the mouse pointer, relative to the left of the owner window
				int           y;      ///< Y position of the mouse pointer, relative to the top of the owner window
			};

			////////////////////////////////////////////////////////////
			/// \brief Mouse wheel events parameters (MouseWheelMoved)
			///
			////////////////////////////////////////////////////////////
			struct MouseWheelEvent
			{
				int delta; ///< Number of ticks the wheel has moved (positive is up, negative is down)
				int x;     ///< X position of the mouse pointer, relative to the left of the owner window
				int y;     ///< Y position of the mouse pointer, relative to the top of the owner window
			};

			////////////////////////////////////////////////////////////
			/// \brief Joystick connection events parameters
			///        (JoystickConnected, JoystickDisconnected)
			///
			////////////////////////////////////////////////////////////
			struct JoystickConnectEvent
			{
				unsigned int joystickId; ///< Index of the joystick (in range [0 .. Joystick::Count - 1])
			};

			////////////////////////////////////////////////////////////
			/// \brief Joystick axis move event parameters (JoystickMoved)
			///
			////////////////////////////////////////////////////////////
			struct JoystickMoveEvent
			{
				EJoystickAxis  AsJoystickAxis()const { return EJoystickAxis::CreateFromInt(axis); }

				unsigned int	joystickId; ///< Index of the joystick (in range [0 .. Joystick::Count - 1])
				int				axis;       ///< Axis on which the joystick moved
				float			position;   ///< New position on the axis (in range [-100 .. 100])
			};

			////////////////////////////////////////////////////////////
			/// \brief Joystick buttons events parameters
			///        (JoystickButtonPressed, JoystickButtonReleased)
			///
			////////////////////////////////////////////////////////////
			struct JoystickButtonEvent
			{
				unsigned int joystickId; ///< Index of the joystick (
				unsigned int button;     ///< Index of the button that has been pressed 
			};

			////////////////////////////////////////////////////////////
			/// \brief XboxConnectEvent connection events parameters
			///
			////////////////////////////////////////////////////////////
			struct ConsoleGamepadConnectEvent
			{
				unsigned int gamepadIndex; ///< Index of the gamepad 
			};

			////////////////////////////////////////////////////////////
			/// \brief ConsoleGamepad axis move event parameters
			///
			////////////////////////////////////////////////////////////
			struct ConsoleGamepadMoveEvent
			{
				ConsoleGamepad::EButtonID AsButtonId()const { return ConsoleGamepad::EButtonID::CreateFromInt(button); }

				unsigned int	gamepadIndex;
				int button;
				float x;
				float y;
			};

			////////////////////////////////////////////////////////////
			/// \brief ConsoleGamepad buttons events parameters
			///
			////////////////////////////////////////////////////////////
			struct ConsoleGamepadButtonEvent
			{
				ConsoleGamepad::EButtonID AsButtonId()const { return ConsoleGamepad::EButtonID::CreateFromInt(button); }

				unsigned int gamepadIndex;
				int button;     
			};

			////////////////////////////////////////////////////////////
			/// \brief ConsoleGamepad buttons events parameters
			///
			////////////////////////////////////////////////////////////
			struct ConsoleGamepadAnalogueTriggerEvent
			{
				ConsoleGamepad::EButtonID AsButtonId()const { return ConsoleGamepad::EButtonID::CreateFromInt(button); }

				unsigned int gamepadIndex; 
				int button;   
				float value;
			};

			////////////////////////////////////////////////////////////
			/// \brief Enumeration of the different types of events
			///
			////////////////////////////////////////////////////////////
			CLASSEDENUM(EType, \
				CE_ITEMVAL(kClosed, 0)\
				CE_ITEM(kResized)\
				CE_ITEM(kLostFocus)\
				CE_ITEM(kGainedFocus)\
				CE_ITEM(kTextEntered)\
				CE_ITEM(kKeyPressed)\
				CE_ITEM(kKeyReleased)\
				CE_ITEM(kMouseWheelMoved)\
				CE_ITEM(kMouseButtonPressed)\
				CE_ITEM(kMouseButtonReleased)\
				CE_ITEM(kMouseMoved)\
				CE_ITEM(kMouseEntered)\
				CE_ITEM(kMouseLeft)\
				CE_ITEM(kJoystickButtonPressed)\
				CE_ITEM(kJoystickButtonReleased)\
				CE_ITEM(kJoystickMoved)\
				CE_ITEM(kJoystickConnected)\
				CE_ITEM(kJoystickDisconnected)\
				CE_ITEM(kConsoleGamepadButtonPressed)\
				CE_ITEM(kConsoleGamepadButtonReleased)\
				CE_ITEM(kConsoleGamepadAnalogStickMove)\
				CE_ITEM(kConsoleGamepadAnalogTriggers)\
				CE_ITEM(kConsoleGamepadConnected)\
				CE_ITEM(kConsoleGamepadDisconnected)\
				, kClosed \
				);
			
			////////////////////////////////////////////////////////////
			// Member data
			////////////////////////////////////////////////////////////
			EType type; ///< Type of the event

			union
			{
				SizeEvent            size;            ///< Size event parameters (Event::Resized)
				KeyEvent             key;             ///< Key event parameters (Event::KeyPressed, Event::KeyReleased)
				TextEvent            text;            ///< Text event parameters (Event::TextEntered)
				MouseMoveEvent       mouseMove;       ///< Mouse move event parameters (Event::MouseMoved)
				MouseButtonEvent     mouseButton;     ///< Mouse button event parameters (Event::MouseButtonPressed, Event::MouseButtonReleased)
				MouseWheelEvent      mouseWheel;      ///< Mouse wheel event parameters (Event::MouseWheelMoved)
				JoystickConnectEvent joystickConnect;
				JoystickMoveEvent	 joystickMove;
				JoystickButtonEvent	 joystickButton;
				ConsoleGamepadConnectEvent consoleGamepadConnectEvent;
				ConsoleGamepadMoveEvent consoleGamepadMoveEvent;
				ConsoleGamepadButtonEvent consoleGamepadButtonEvent;
				ConsoleGamepadAnalogueTriggerEvent consoleGamepadAnalogueTriggerEvent;
			};
		};
	}
}