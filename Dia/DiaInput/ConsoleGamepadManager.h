////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleGamepadManager.h: Files to control a Xbox360 gamepad
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/ConsoleGamepad.h"
#include "DiaInput/IInputSource.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>
namespace Dia
{
	namespace Input
	{
		/// @brief IInputSource implementation for Xbox controller management
		///
		/// ConsoleGamepadManager pools up to 8 gamepads and automatically detects
		/// hot-plugging (connection/disconnection). When a gamepad connects, a
		/// kConsoleGamepadConnected event is fired. When disconnected, a
		/// kConsoleGamepadDisconnected event is fired.
		///
		/// **Features:**
		/// - Hot-plug detection each frame
		/// - Automatic button press/release event generation
		/// - Analog stick movement events (when outside deadzone)
		/// - Trigger events (when pressed beyond threshold)
		///
		/// **Capacity:** Up to 8 gamepads (pool size)
		///
		/// **Usage:**
		/// @code
		/// ConsoleGamepadManager gamepadManager;
		/// inputSourceManager.AddInputSource(&gamepadManager);
		///
		/// // Each frame:
		/// inputSourceManager.StartFrame();
		/// EventData events;
		/// inputSourceManager.Update(events);  // Polls gamepads
		/// inputSourceManager.EndFrame();
		///
		/// // Process events
		/// for (unsigned int i = 0; i < events.Size(); i++)
		/// {
		///     if (events[i].type == Event::EType::kConsoleGamepadButtonPressed)
		///     {
		///         unsigned int gamepadIdx = events[i].consoleGamepadButtonEvent.gamepadIndex;
		///         ConsoleGamepad::EButtonID button = events[i].consoleGamepadButtonEvent.AsButtonId();
		///     }
		/// }
		/// @endcode
		///
		/// @note Automatically updates all connected gamepads during Poll()
		////////////////////////////////////////////////////////////////////////////////
		class ConsoleGamepadManager : public  Dia::Input::IInputSource
		{
		public:
			typedef Dia::Core::Containers::DynamicArrayC<ConsoleGamepad*, 8> ConsoleGamepadActiveList;
			typedef Dia::Core::Containers::ArrayC<ConsoleGamepad, 8> ConsoleGamepadPool;

			/// @brief Constructor - initializes gamepad pool
			ConsoleGamepadManager();

			/// @brief Called at the beginning of input polling frame (no-op)
			virtual void StartFrame()override;

			/// @brief Poll all connected gamepads and generate input events
			///
			/// @param outStream Event buffer to populate with gamepad events
			///
			/// Generates events for:
			/// - Button presses/releases
			/// - Analog stick movement (when outside deadzone)
			/// - Trigger presses (when beyond threshold)
			/// - Gamepad connection/disconnection
			virtual void Poll(EventData& outStream)override;;

			/// @brief Called at end of input polling frame - refreshes gamepad state
			///
			/// Updates button state history for edge detection (press/release).
			virtual void EndFrame()override;

			/// @brief Check if a gamepad is registered as active
			///
			/// @param gamepad Gamepad to check
			/// @return true if gamepad is in active list, false otherwise
			bool IsRegisteredAsActive(const ConsoleGamepad& gamepad)const;

		private:
			ConsoleGamepadActiveList mGamepadActiveList;  ///< Currently connected gamepads
			ConsoleGamepadPool mGamepadPool;              ///< Pool of up to 8 gamepads
		};
	}
}