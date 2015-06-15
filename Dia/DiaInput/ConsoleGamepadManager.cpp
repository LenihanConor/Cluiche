////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleGamepadManager
////////////////////////////////////////////////////////////////////////////////
#include "DiaInput/ConsoleGamepadManager.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Input
	{
		//-----------------------------------------------------------------------------
		ConsoleGamepadManager::ConsoleGamepadManager()
		{
			for (unsigned int i = 0; i < mGamepadPool.Size(); i++)
			{
				mGamepadPool[i].Initialize(i);
			}

			for (unsigned int i = 0; i < mGamepadActiveList.Size(); i++)
			{
				mGamepadActiveList[i] = nullptr;
			}
		}

		//-----------------------------------------------------------------------------
		void ConsoleGamepadManager::StartFrame()
		{

		}

		//-----------------------------------------------------------------------------
		void ConsoleGamepadManager::Poll(EventStream& outStream)
		{
			// Test for any disconnection
			for (int i = mGamepadActiveList.Size() -1; i >= 0; i--)
			{
				DIA_ASSERT(mGamepadActiveList[i], "The gamepad should not be null");

				if (!mGamepadActiveList[i]->IsConnected())
				{
					Event disconnectEvent;
					disconnectEvent.type = Event::EType::kConsoleGamepadDisconnected;
					disconnectEvent.consoleGamepadConnectEvent.gamepadIndex = mGamepadActiveList[i]->GetIndex();
					outStream.Add(disconnectEvent);

					mGamepadActiveList.RemoveAt(i);
				}
			}

			// Test for activation
			for (unsigned int i = 0; i < mGamepadPool.Size(); i++)
			{
				if (!IsRegisteredAsActive(mGamepadPool[i]))
				{
					if (mGamepadPool[i].IsConnected())
					{
						Event connectEvent;
						connectEvent.type = Event::EType::kConsoleGamepadConnected;
						connectEvent.consoleGamepadConnectEvent.gamepadIndex = mGamepadPool[i].GetIndex();
						outStream.Add(connectEvent);

						mGamepadActiveList.Add(&mGamepadPool[i]);
					}
				}
			}

			// Test for Button presses
			for (unsigned int i = 0; i < mGamepadActiveList.Size(); i++)
			{
				const ConsoleGamepad* gamepad = mGamepadActiveList[i];

				DIA_ASSERT(gamepad, "The gamepad should not be null");

				// Test analogue stick movement
				if (!gamepad->IsLStickInDeadzone())
				{
					Event analogueStickMoveEvent;
					analogueStickMoveEvent.type = Event::EType::kConsoleGamepadAnalogStickMove;
					analogueStickMoveEvent.consoleGamepadMoveEvent.gamepadIndex = gamepad->GetIndex();
					analogueStickMoveEvent.consoleGamepadMoveEvent.button = ConsoleGamepad::EButtonID::L_Thumbstick;
					analogueStickMoveEvent.consoleGamepadMoveEvent.x = gamepad->GetLeftStickX();
					analogueStickMoveEvent.consoleGamepadMoveEvent.y = gamepad->GetLeftStickY();
					outStream.Add(analogueStickMoveEvent);
				}

				// Test analogue stick movement
				if (!gamepad->IsRStickInDeadzone())
				{
					Event analogueStickMoveEvent;
					analogueStickMoveEvent.type = Event::EType::kConsoleGamepadAnalogStickMove;
					analogueStickMoveEvent.consoleGamepadMoveEvent.gamepadIndex = gamepad->GetIndex();
					analogueStickMoveEvent.consoleGamepadMoveEvent.button = ConsoleGamepad::EButtonID::R_Thumbstick;
					analogueStickMoveEvent.consoleGamepadMoveEvent.x = gamepad->GetRightStickX();
					analogueStickMoveEvent.consoleGamepadMoveEvent.y = gamepad->GetRightStickY();
					outStream.Add(analogueStickMoveEvent);
				}

				// Test triggers
				if (gamepad->GetLeftTrigger() > 0.0f)
				{
					Event analogueTriggerEvent;
					analogueTriggerEvent.type = Event::EType::kConsoleGamepadAnalogTriggers;
					analogueTriggerEvent.consoleGamepadAnalogueTriggerEvent.gamepadIndex = gamepad->GetIndex();
					analogueTriggerEvent.consoleGamepadAnalogueTriggerEvent.button = ConsoleGamepad::EButtonID::L_Trigger;
					analogueTriggerEvent.consoleGamepadAnalogueTriggerEvent.value = gamepad->GetLeftTrigger();
					outStream.Add(analogueTriggerEvent);
				}

				// Test triggers
				if (gamepad->GetRightTrigger() > 0.0f)
				{
					Event analogueTriggerEvent;
					analogueTriggerEvent.type = Event::EType::kConsoleGamepadAnalogTriggers;
					analogueTriggerEvent.consoleGamepadAnalogueTriggerEvent.gamepadIndex = gamepad->GetIndex();
					analogueTriggerEvent.consoleGamepadAnalogueTriggerEvent.button = ConsoleGamepad::EButtonID::R_Trigger;
					analogueTriggerEvent.consoleGamepadAnalogueTriggerEvent.value = gamepad->GetRightTrigger();
					outStream.Add(analogueTriggerEvent);
				}
				
				// Test digital buttons
				for (unsigned int i = 0; i < ConsoleGamepad::GetMaxDigitalButtons(); i++)
				{
					if (gamepad->IsButtonPressed(i))
					{
						Event buttonPressedEvent;
						buttonPressedEvent.type = Event::EType::kConsoleGamepadButtonPressed;
						buttonPressedEvent.consoleGamepadButtonEvent.gamepadIndex = gamepad->GetIndex();
						buttonPressedEvent.consoleGamepadButtonEvent.button = i;
						outStream.Add(buttonPressedEvent);
					}

					if (gamepad->IsButtonReleased(i))
					{
						Event buttonPressedEvent;
						buttonPressedEvent.type = Event::EType::kConsoleGamepadButtonReleased;
						buttonPressedEvent.consoleGamepadButtonEvent.gamepadIndex = gamepad->GetIndex();
						buttonPressedEvent.consoleGamepadButtonEvent.button = i;
						outStream.Add(buttonPressedEvent);
					}
				}
			}
		}

		//-----------------------------------------------------------------------------
		void ConsoleGamepadManager::EndFrame()
		{
			// Set the state of the gamepad so that it knwo if its state changed last frame
			for (unsigned int i = 0; i < mGamepadActiveList.Size(); i++)
			{
				mGamepadActiveList[i]->RefreshState();
			}
		}

		//-----------------------------------------------------------------------------
		bool ConsoleGamepadManager::IsRegisteredAsActive(const ConsoleGamepad& gamepad)const
		{
			// Test for Button presses
			for (unsigned int i = 0; i < mGamepadActiveList.Size(); i++)
			{
				DIA_ASSERT(mGamepadActiveList[i], "The gamepad should not be null");
				if (gamepad.GetIndex() == mGamepadActiveList[i]->GetIndex())
				{
					return true;
				}
			}

			return false;
		}
	}
}