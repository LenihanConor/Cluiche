////////////////////////////////////////////////////////////////////////////////
// Filename: InputState.h - Query API for current input state
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/Event.h"
#include "DiaInput/EventData.h"
#include "DiaInput/EKey.h"
#include "DiaInput/EMouseButton.h"
#include "DiaInput/ConsoleGamepad.h"

namespace Dia
{
	namespace Input
	{
		/// @brief Query API for current input state
		///
		/// InputState processes EventData each frame and maintains queryable state
		/// for keys, mouse buttons, and gamepads. This provides a convenient
		/// "Is key down?" API instead of event-driven "key pressed" events.
		///
		/// **Usage:**
		/// @code
		/// InputState state;
		///
		/// // Each frame:
		/// EventData events;
		/// inputSourceManager.Update(events);
		/// state.ProcessEvents(events);
		///
		/// // Query state:
		/// if (state.IsKeyDown(EKey::W))
		///     player.MoveForward(deltaTime);
		///
		/// if (state.WasKeyPressed(EKey::Space))  // Edge detection
		///     player.Jump();
		///
		/// // At frame end:
		/// state.ClearFrameState();
		/// @endcode
		///
		/// **Frame vs Persistent State:**
		/// - `IsKeyDown()`: Persistent (true while held)
		/// - `WasKeyPressed()`: Frame-only (true only on press frame)
		/// - `WasKeyReleased()`: Frame-only (true only on release frame)
		////////////////////////////////////////////////////////////////////////////////
		class InputState
		{
		public:
			static const unsigned int kMaxKeys = 256;
			static const unsigned int kMaxMouseButtons = 8;
			static const unsigned int kMaxGamepads = 8;
			static const unsigned int kMaxJoysticks = 8;
			static const unsigned int kMaxJoystickButtons = 32;
			static const unsigned int kMaxJoystickAxes = 8;

			InputState()
				: mMouseX(0), mMouseY(0), mMouseWheelDelta(0)
			{
				// Initialize all state to false/zero
				for (unsigned int i = 0; i < kMaxKeys; i++)
				{
					mKeyDown[i] = false;
					mKeyPressed[i] = false;
					mKeyReleased[i] = false;
				}

				for (unsigned int i = 0; i < kMaxMouseButtons; i++)
				{
					mMouseButtonDown[i] = false;
					mMouseButtonPressed[i] = false;
					mMouseButtonReleased[i] = false;
				}

				for (unsigned int i = 0; i < kMaxGamepads; i++)
				{
					mGamepadConnected[i] = false;
					for (unsigned int j = 0; j < 16; j++)
					{
						mGamepadButtonDown[i][j] = false;
					}
					for (unsigned int j = 0; j < 4; j++)
					{
						mGamepadAxisValues[i][j] = 0.0f;
					}
				}

				for (unsigned int i = 0; i < kMaxJoysticks; i++)
				{
					mJoystickConnected[i] = false;
					for (unsigned int j = 0; j < kMaxJoystickButtons; j++)
					{
						mJoystickButtonDown[i][j] = false;
					}
					for (unsigned int j = 0; j < kMaxJoystickAxes; j++)
					{
						mJoystickAxisValues[i][j] = 0.0f;
					}
				}
			}

			/// @brief Process events to update state
			///
			/// @param events EventData from InputSourceManager
			///
			/// Call this each frame after polling input sources.
			void ProcessEvents(const EventData& events)
			{
				for (unsigned int i = 0; i < events.Size(); i++)
				{
					const Event& evt = events[i];

					switch (evt.type)
					{
					case Event::EType::kKeyPressed:
					{
						int keyCode = evt.key.code;
						if (keyCode >= 0 && keyCode < kMaxKeys)
						{
							if (!mKeyDown[keyCode])
							{
								mKeyPressed[keyCode] = true;
							}
							mKeyDown[keyCode] = true;
						}
						break;
					}

					case Event::EType::kKeyReleased:
					{
						int keyCode = evt.key.code;
						if (keyCode >= 0 && keyCode < kMaxKeys)
						{
							mKeyDown[keyCode] = false;
							mKeyReleased[keyCode] = true;
						}
						break;
					}

					case Event::EType::kMouseButtonPressed:
					{
						int button = evt.mouseButton.button;
						if (button >= 0 && button < kMaxMouseButtons)
						{
							if (!mMouseButtonDown[button])
							{
								mMouseButtonPressed[button] = true;
							}
							mMouseButtonDown[button] = true;
						}
						break;
					}

					case Event::EType::kMouseButtonReleased:
					{
						int button = evt.mouseButton.button;
						if (button >= 0 && button < kMaxMouseButtons)
						{
							mMouseButtonDown[button] = false;
							mMouseButtonReleased[button] = true;
						}
						break;
					}

					case Event::EType::kMouseMoved:
						mMouseX = evt.mouseMove.x;
						mMouseY = evt.mouseMove.y;
						break;

					case Event::EType::kMouseWheelMoved:
						mMouseWheelDelta += evt.mouseWheel.delta;
						break;

					case Event::EType::kConsoleGamepadConnected:
					{
						unsigned int idx = evt.consoleGamepadConnectEvent.gamepadIndex;
						if (idx < kMaxGamepads)
						{
							mGamepadConnected[idx] = true;
						}
						break;
					}

					case Event::EType::kConsoleGamepadDisconnected:
					{
						unsigned int idx = evt.consoleGamepadConnectEvent.gamepadIndex;
						if (idx < kMaxGamepads)
						{
							mGamepadConnected[idx] = false;
						}
						break;
					}

					case Event::EType::kConsoleGamepadButtonPressed:
					{
						unsigned int idx = evt.consoleGamepadButtonEvent.gamepadIndex;
						int button = evt.consoleGamepadButtonEvent.button;
						if (idx < kMaxGamepads && button < 16)
						{
							mGamepadButtonDown[idx][button] = true;
						}
						break;
					}

					case Event::EType::kConsoleGamepadButtonReleased:
					{
						unsigned int idx = evt.consoleGamepadButtonEvent.gamepadIndex;
						int button = evt.consoleGamepadButtonEvent.button;
						if (idx < kMaxGamepads && button < 16)
						{
							mGamepadButtonDown[idx][button] = false;
						}
						break;
					}

					case Event::EType::kConsoleGamepadAnalogStickMove:
					{
						unsigned int idx = evt.consoleGamepadMoveEvent.gamepadIndex;
						int stick = evt.consoleGamepadMoveEvent.button;
						if (idx < kMaxGamepads)
						{
							if (stick == ConsoleGamepad::EButtonID::L_Thumbstick)
							{
								mGamepadAxisValues[idx][0] = evt.consoleGamepadMoveEvent.x;
								mGamepadAxisValues[idx][1] = evt.consoleGamepadMoveEvent.y;
							}
							else if (stick == ConsoleGamepad::EButtonID::R_Thumbstick)
							{
								mGamepadAxisValues[idx][2] = evt.consoleGamepadMoveEvent.x;
								mGamepadAxisValues[idx][3] = evt.consoleGamepadMoveEvent.y;
							}
						}
						break;
					}

					case Event::EType::kJoystickConnected:
					{
						unsigned int idx = evt.joystickConnect.joystickId;
						if (idx < kMaxJoysticks)
						{
							mJoystickConnected[idx] = true;
						}
						break;
					}

					case Event::EType::kJoystickDisconnected:
					{
						unsigned int idx = evt.joystickConnect.joystickId;
						if (idx < kMaxJoysticks)
						{
							mJoystickConnected[idx] = false;
						}
						break;
					}

					case Event::EType::kJoystickButtonPressed:
					{
						unsigned int idx = evt.joystickButton.joystickId;
						unsigned int button = evt.joystickButton.button;
						if (idx < kMaxJoysticks && button < kMaxJoystickButtons)
						{
							mJoystickButtonDown[idx][button] = true;
						}
						break;
					}

					case Event::EType::kJoystickButtonReleased:
					{
						unsigned int idx = evt.joystickButton.joystickId;
						unsigned int button = evt.joystickButton.button;
						if (idx < kMaxJoysticks && button < kMaxJoystickButtons)
						{
							mJoystickButtonDown[idx][button] = false;
						}
						break;
					}

					case Event::EType::kJoystickMoved:
					{
						unsigned int idx = evt.joystickMove.joystickId;
						int axis = evt.joystickMove.axis;
						if (idx < kMaxJoysticks && axis >= 0 && axis < kMaxJoystickAxes)
						{
							mJoystickAxisValues[idx][axis] = evt.joystickMove.position;
						}
						break;
					}

					default:
						break;
					}
				}
			}

			// Keyboard queries
			bool IsKeyDown(EKey key) const
			{
				int keyCode = key.AsInt();
				return (keyCode >= 0 && keyCode < kMaxKeys) ? mKeyDown[keyCode] : false;
			}

			bool WasKeyPressed(EKey key) const
			{
				int keyCode = key.AsInt();
				return (keyCode >= 0 && keyCode < kMaxKeys) ? mKeyPressed[keyCode] : false;
			}

			bool WasKeyReleased(EKey key) const
			{
				int keyCode = key.AsInt();
				return (keyCode >= 0 && keyCode < kMaxKeys) ? mKeyReleased[keyCode] : false;
			}

			// Mouse queries
			bool IsMouseButtonDown(EMouseButton button) const
			{
				int buttonCode = button.AsInt();
				return (buttonCode >= 0 && buttonCode < kMaxMouseButtons) ? mMouseButtonDown[buttonCode] : false;
			}

			bool WasMouseButtonPressed(EMouseButton button) const
			{
				int buttonCode = button.AsInt();
				return (buttonCode >= 0 && buttonCode < kMaxMouseButtons) ? mMouseButtonPressed[buttonCode] : false;
			}

			bool WasMouseButtonReleased(EMouseButton button) const
			{
				int buttonCode = button.AsInt();
				return (buttonCode >= 0 && buttonCode < kMaxMouseButtons) ? mMouseButtonReleased[buttonCode] : false;
			}

			void GetMousePosition(int& x, int& y) const
			{
				x = mMouseX;
				y = mMouseY;
			}

			int GetMouseWheelDelta() const
			{
				return mMouseWheelDelta;
			}

			// Gamepad queries
			bool IsGamepadConnected(unsigned int index) const
			{
				return (index < kMaxGamepads) ? mGamepadConnected[index] : false;
			}

			bool IsGamepadButtonDown(unsigned int index, ConsoleGamepad::EButtonID button) const
			{
				int buttonCode = button.AsInt();
				return (index < kMaxGamepads && buttonCode < 16) ? mGamepadButtonDown[index][buttonCode] : false;
			}

			void GetGamepadAnalogStick(unsigned int index, ConsoleGamepad::EButtonID stick, float& x, float& y) const
			{
				if (index >= kMaxGamepads)
				{
					x = 0.0f;
					y = 0.0f;
					return;
				}

				int stickCode = stick.AsInt();
				if (stickCode == ConsoleGamepad::EButtonID::L_Thumbstick)
				{
					x = mGamepadAxisValues[index][0];
					y = mGamepadAxisValues[index][1];
				}
				else if (stickCode == ConsoleGamepad::EButtonID::R_Thumbstick)
				{
					x = mGamepadAxisValues[index][2];
					y = mGamepadAxisValues[index][3];
				}
				else
				{
					x = 0.0f;
					y = 0.0f;
				}
			}

			// Joystick queries
			bool IsJoystickConnected(unsigned int index) const
			{
				return (index < kMaxJoysticks) ? mJoystickConnected[index] : false;
			}

			bool IsJoystickButtonDown(unsigned int index, unsigned int button) const
			{
				return (index < kMaxJoysticks && button < kMaxJoystickButtons) ? mJoystickButtonDown[index][button] : false;
			}

			float GetJoystickAxisValue(unsigned int index, EJoystickAxis axis) const
			{
				int axisCode = axis.AsInt();
				return (index < kMaxJoysticks && axisCode >= 0 && axisCode < kMaxJoystickAxes) ? mJoystickAxisValues[index][axisCode] : 0.0f;
			}

			void GetJoystickPosition(unsigned int index, float& x, float& y) const
			{
				if (index < kMaxJoysticks)
				{
					x = mJoystickAxisValues[index][EJoystickAxis::X];
					y = mJoystickAxisValues[index][EJoystickAxis::Y];
				}
				else
				{
					x = 0.0f;
					y = 0.0f;
				}
			}

			/// @brief Clear frame-only state (pressed/released flags, mouse wheel delta)
			///
			/// Call this at the end of each frame to reset edge-detection flags.
			void ClearFrameState()
			{
				for (unsigned int i = 0; i < kMaxKeys; i++)
				{
					mKeyPressed[i] = false;
					mKeyReleased[i] = false;
				}

				for (unsigned int i = 0; i < kMaxMouseButtons; i++)
				{
					mMouseButtonPressed[i] = false;
					mMouseButtonReleased[i] = false;
				}

				mMouseWheelDelta = 0;
			}

		private:
			// Keyboard state
			bool mKeyDown[kMaxKeys];
			bool mKeyPressed[kMaxKeys];
			bool mKeyReleased[kMaxKeys];

			// Mouse state
			bool mMouseButtonDown[kMaxMouseButtons];
			bool mMouseButtonPressed[kMaxMouseButtons];
			bool mMouseButtonReleased[kMaxMouseButtons];
			int mMouseX;
			int mMouseY;
			int mMouseWheelDelta;

			// Gamepad state
			bool mGamepadConnected[kMaxGamepads];
			bool mGamepadButtonDown[kMaxGamepads][16];  // 16 buttons per gamepad
			float mGamepadAxisValues[kMaxGamepads][4];  // LX, LY, RX, RY per gamepad

			// Joystick state
			bool mJoystickConnected[kMaxJoysticks];
			bool mJoystickButtonDown[kMaxJoysticks][kMaxJoystickButtons];  // 32 buttons per joystick
			float mJoystickAxisValues[kMaxJoysticks][kMaxJoystickAxes];  // 8 axes per joystick (X, Y, Z, R, U, V, PovX, PovY)
		};
	}
}
