#ifndef DIA_INPUT_LEGACY_EVENT_CONVERTER_H
#define DIA_INPUT_LEGACY_EVENT_CONVERTER_H

#include <DiaCore/Architecture/Events/Event.h>
#include <DiaCore/Architecture/Events/EventDispatcher.h>
#include "DiaInput/Event.h"
#include "DiaInput/EventData.h"
#include "DiaInput/Events/KeyboardEvents.h"
#include "DiaInput/Events/MouseEvents.h"
#include "DiaInput/Events/GamepadEvents.h"
#include "DiaInput/Events/JoystickEvents.h"

namespace Dia
{
	namespace Input
	{
		namespace Events
		{
			/// @brief Converts legacy union-based Events to modern typed events
			///
			/// LegacyEventConverter provides a bridge between the old Event union
			/// system and the new type-safe event classes. This allows gradual
			/// migration from legacy to modern events.
			///
			/// **Usage:**
			/// @code
			/// EventData legacyEvents;
			/// inputSourceManager.Update(legacyEvents);
			///
			/// Core::Events::EventDispatcher dispatcher;
			/// LegacyEventConverter::ConvertAndDispatch(legacyEvents, dispatcher);
			///
			/// // Modern event handlers receive typed events
			/// @endcode
			////////////////////////////////////////////////////////////////////////////////
			class LegacyEventConverter
			{
			public:
				/// @brief Convert a single legacy event to modern event
				///
				/// @param legacyEvent Union-based legacy event
				/// @return New heap-allocated modern event, or nullptr if unsupported
				///
				/// @note Caller is responsible for deleting returned event
				static Core::Events::Event* ToModernEvent(const Event& legacyEvent)
				{
					switch (static_cast<int>(legacyEvent.type.m_Value))
					{
					case Event::EType::kKeyPressed:
						return new KeyPressedEvent(
							legacyEvent.key.AsKey(),
							legacyEvent.key.alt,
							legacyEvent.key.control,
							legacyEvent.key.shift,
							legacyEvent.key.system
						);

					case Event::EType::kKeyReleased:
						return new KeyReleasedEvent(
							legacyEvent.key.AsKey(),
							legacyEvent.key.alt,
							legacyEvent.key.control,
							legacyEvent.key.shift,
							legacyEvent.key.system
						);

					case Event::EType::kTextEntered:
						return new TextEnteredEvent(
							legacyEvent.text.unicode
						);

					case Event::EType::kMouseButtonPressed:
						return new MouseButtonPressedEvent(
							legacyEvent.mouseButton.AsMouseButton(),
							legacyEvent.mouseButton.x,
							legacyEvent.mouseButton.y
						);

					case Event::EType::kMouseButtonReleased:
						return new MouseButtonReleasedEvent(
							legacyEvent.mouseButton.AsMouseButton(),
							legacyEvent.mouseButton.x,
							legacyEvent.mouseButton.y
						);

					case Event::EType::kMouseMoved:
						return new MouseMovedEvent(
							legacyEvent.mouseMove.x,
							legacyEvent.mouseMove.y
						);

					case Event::EType::kMouseWheelMoved:
						return new MouseWheelMovedEvent(
							legacyEvent.mouseWheel.delta,
							legacyEvent.mouseWheel.x,
							legacyEvent.mouseWheel.y
						);

					case Event::EType::kMouseEntered:
						return new MouseEnteredEvent();

					case Event::EType::kMouseLeft:
						return new MouseLeftEvent();

					case Event::EType::kConsoleGamepadButtonPressed:
						return new GamepadButtonPressedEvent(
							legacyEvent.consoleGamepadButtonEvent.gamepadIndex,
							legacyEvent.consoleGamepadButtonEvent.AsButtonId()
						);

					case Event::EType::kConsoleGamepadButtonReleased:
						return new GamepadButtonReleasedEvent(
							legacyEvent.consoleGamepadButtonEvent.gamepadIndex,
							legacyEvent.consoleGamepadButtonEvent.AsButtonId()
						);

					case Event::EType::kConsoleGamepadAnalogStickMove:
						return new GamepadAnalogStickMoveEvent(
							legacyEvent.consoleGamepadMoveEvent.gamepadIndex,
							legacyEvent.consoleGamepadMoveEvent.AsButtonId(),
							legacyEvent.consoleGamepadMoveEvent.x,
							legacyEvent.consoleGamepadMoveEvent.y
						);

					case Event::EType::kConsoleGamepadAnalogTriggers:
						return new GamepadTriggerEvent(
							legacyEvent.consoleGamepadAnalogueTriggerEvent.gamepadIndex,
							legacyEvent.consoleGamepadAnalogueTriggerEvent.AsButtonId(),
							legacyEvent.consoleGamepadAnalogueTriggerEvent.value
						);

					case Event::EType::kConsoleGamepadConnected:
						return new GamepadConnectedEvent(
							legacyEvent.consoleGamepadConnectEvent.gamepadIndex
						);

					case Event::EType::kConsoleGamepadDisconnected:
						return new GamepadDisconnectedEvent(
							legacyEvent.consoleGamepadConnectEvent.gamepadIndex
						);

					case Event::EType::kJoystickButtonPressed:
						return new JoystickButtonPressedEvent(
							legacyEvent.joystickButton.joystickId,
							legacyEvent.joystickButton.button
						);

					case Event::EType::kJoystickButtonReleased:
						return new JoystickButtonReleasedEvent(
							legacyEvent.joystickButton.joystickId,
							legacyEvent.joystickButton.button
						);

					case Event::EType::kJoystickMoved:
						return new JoystickAxisMovedEvent(
							legacyEvent.joystickMove.joystickId,
							legacyEvent.joystickMove.AsJoystickAxis(),
							legacyEvent.joystickMove.position
						);

					case Event::EType::kJoystickConnected:
						return new JoystickConnectedEvent(
							legacyEvent.joystickConnect.joystickId
						);

					case Event::EType::kJoystickDisconnected:
						return new JoystickDisconnectedEvent(
							legacyEvent.joystickConnect.joystickId
						);

					// Unsupported types (window events)
					default:
						return nullptr;
					}
				}

				/// @brief Convert and dispatch all legacy events to modern dispatcher
				///
				/// @param legacyEvents EventData containing union-based events
				/// @param dispatcher EventDispatcher to queue modern events into
				///
				/// Converts all supported legacy events and queues them to the
				/// dispatcher. Unsupported events are skipped.
				static void ConvertAndDispatch(const EventData& legacyEvents,
					Core::Events::EventDispatcher& dispatcher)
				{
					for (unsigned int i = 0; i < legacyEvents.Size(); i++)
					{
						Core::Events::Event* modernEvent = ToModernEvent(legacyEvents[i]);
						if (modernEvent)
						{
							dispatcher.QueueEvent(modernEvent);
						}
					}

					// Process the queue immediately to trigger callbacks
					dispatcher.ProcessQueue();
				}
			};
		}
	}
}

#endif // DIA_INPUT_LEGACY_EVENT_CONVERTER_H
