////////////////////////////////////////////////////////////////////////////////
// Filename: InputSourceManager.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/EventData.h"
#include "DiaInput/IInputSource.h"

// Forward declare for modern event support
namespace Dia { namespace Core { namespace Events { class EventDispatcher; } } }

namespace Dia
{
	namespace Input
	{
		/// @brief Aggregates and polls multiple input sources
		///
		/// InputSourceManager coordinates multiple IInputSource implementations
		/// (SFML keyboard/mouse, XInput gamepads, etc.) and collects their events
		/// into a unified EventData stream each frame.
		///
		/// **Lifecycle:** StartFrame() → Update() → EndFrame()
		///
		/// **Usage Example:**
		/// @code
		/// InputSourceManager manager;
		/// SFMLInputSource sfmlSource;
		/// ConsoleGamepadManager gamepadManager;
		///
		/// manager.AddInputSource(&sfmlSource);
		/// manager.AddInputSource(&gamepadManager);
		///
		/// // Each frame:
		/// manager.StartFrame();
		/// EventData events;
		/// manager.Update(events);  // Polls all sources
		/// manager.EndFrame();
		/// @endcode
		///
		/// **Capacity:** Supports up to 8 input sources.
		////////////////////////////////////////////////////////////////////////////////
		class InputSourceManager
		{
		public:
			typedef Dia::Core::Containers::DynamicArrayC<IInputSource*, 8> InputSourceList;

			/// @brief Default constructor
			InputSourceManager();

			/// @brief Register an input source for polling
			///
			/// @param inputSource Pointer to input source (must remain valid)
			///
			/// @note Does not take ownership - caller manages lifetime
			void AddInputSource(IInputSource* inputSource);

			/// @brief Unregister an input source
			///
			/// @param inputSource Pointer to previously registered source
			///
			/// @warning Asserts if source was not found
			void RemoveInputSource(IInputSource* inputSource);

			/// @brief Called at the beginning of each input polling frame
			///
			/// Invokes StartFrame() on all registered sources.
			void StartFrame();

			/// @brief Poll all registered input sources
			///
			/// @param outStream Event buffer to populate with events from all sources
			///
			/// @note Sources append to outStream - it is not cleared first
			/// @warning Logs warning if event buffer reaches capacity
			void Update(EventData& outStream);

			/// @brief Called at the end of each input polling frame
			///
			/// Invokes EndFrame() on all registered sources.
			void EndFrame();

			/// @brief Poll all sources and dispatch to modern event system
			///
			/// @param dispatcher EventDispatcher to queue modern typed events
			///
			/// This is an opt-in modernized version of Update() that converts
			/// legacy events to type-safe modern events and dispatches them.
			///
			/// **Usage:**
			/// @code
			/// Core::Events::EventDispatcher dispatcher;
			/// manager.UpdateModern(dispatcher);
			///
			/// // Subscribe to typed events
			/// dispatcher.Subscribe<Input::Events::KeyPressedEvent>([](auto* event) {
			///     if (event->GetKey() == EKey::Escape)
			///         QuitGame();
			/// });
			/// @endcode
			void UpdateModern(Core::Events::EventDispatcher& dispatcher);

		private:
			InputSourceList mInputSourceList;  ///< List of registered input sources
		};
	}
}