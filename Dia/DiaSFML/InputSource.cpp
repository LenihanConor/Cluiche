////////////////////////////////////////////////////////////////////////////////
// Filename: InputSource
////////////////////////////////////////////////////////////////////////////////

#include "DiaSFML\InputSource.h"

#include <SFML/Window/Window.hpp> 
#include <SFML/Window/Event.hpp> 

namespace Dia
{
	namespace SFML
	{
		//------------------------------------------------------------------------------
		// InputSource constructor.
		// Initializes the window context to nullptr and sets the input source mask
		// to listen to all input sources by default.
		InputSource::InputSource()
			: mWindowContext(nullptr)
		{
			// Default to listening to everything
			mListeningToSource.SetAllBits(0xFF);
		}

		//------------------------------------------------------
		// Sets the SFML window context to poll events from.
		// This must be called before polling for input.
		void InputSource::SetWindowContext(sf::Window* window)
		{
			mWindowContext = window;
		}

		//------------------------------------------------------
		// Specifies which input sources to listen for (system, keyboard, mouse, joystick).
		// Uses a BitArray8 mask to enable/disable sources.
		void InputSource::ListenForInputSources(Dia::Core::BitArray8 listeningToSource)
		{
			mListeningToSource = listeningToSource;
		}

		//------------------------------------------------------
		// Polls the SFML window for events, converts them to Dia::Input::Event,
		// and adds them to the output stream if the event type is enabled.
		// MouseMoved events are merged if consecutive.
		void InputSource::Poll(Dia::Input::EventData& outStream)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				outStream.RemoveAll();

				// Poll and handle all available SFML events
				while (const std::optional sfEvent = mWindowContext->pollEvent())
				{
					Dia::Input::Event diaEvent;

					// Convert SFML event to Dia event
					// Each block handles a specific SFML event type
					if (sfEvent->is<sf::Event::Resized>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::Resized>();

						diaEvent.type = Dia::Input::Event::EType::kResized;
						diaEvent.size.height = sfInput->size.x;
						diaEvent.size.width = sfInput->size.y;
					}
					else if (sfEvent->is<sf::Event::KeyPressed>() || sfEvent->is<sf::Event::KeyReleased>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::KeyPressed>(); // Same structure for KeyReleased

						if (sfEvent->is<sf::Event::KeyPressed>())
							diaEvent.type = Dia::Input::Event::EType::kKeyPressed;
						else
							diaEvent.type = Dia::Input::Event::EType::kKeyReleased;

						diaEvent.key.code = static_cast<int>(sfInput->code);
						diaEvent.key.alt = sfInput->alt;
						diaEvent.key.control = sfInput->control;
						diaEvent.key.shift = sfInput->shift;
						diaEvent.key.system = sfInput->system;
					}
					else if (sfEvent->is<sf::Event::TextEntered>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::TextEntered>();
						
						diaEvent.type = Dia::Input::Event::EType::kTextEntered;
						diaEvent.text.unicode = sfInput->unicode;
					}
					else if (sfEvent->is<sf::Event::MouseMoved>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::MouseMoved>();

						diaEvent.type = Dia::Input::Event::EType::kMouseMoved;
						diaEvent.mouseMove.x = sfInput->position.x;
						diaEvent.mouseMove.y = sfInput->position.y;
					}
					else if (sfEvent->is<sf::Event::MouseButtonPressed>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::MouseButtonPressed>(); 

	
						diaEvent.type = Dia::Input::Event::EType::kMouseButtonPressed;					
						diaEvent.mouseButton.button = static_cast<int>(sfInput->button);
						diaEvent.mouseButton.x = sfInput->position.x;
						diaEvent.mouseButton.y = sfInput->position.y;
					}
					else if (sfEvent->is<sf::Event::MouseButtonReleased>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::MouseButtonReleased>();

						diaEvent.type = Dia::Input::Event::EType::kMouseButtonReleased;
						diaEvent.mouseButton.button = static_cast<int>(sfInput->button);
						diaEvent.mouseButton.x = sfInput->position.x;
						diaEvent.mouseButton.y = sfInput->position.y;
					}
					else if (sfEvent->is<sf::Event::JoystickConnected>() || sfEvent->is<sf::Event::JoystickDisconnected>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::JoystickConnected>();

						if (sfEvent->is<sf::Event::JoystickConnected>())
							diaEvent.type = Dia::Input::Event::EType::kJoystickConnected;
						else
							diaEvent.type = Dia::Input::Event::EType::kJoystickDisconnected;

						diaEvent.joystickConnect.joystickId = sfInput->joystickId;
					}
					else if (sfEvent->is<sf::Event::JoystickMoved>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::JoystickMoved>();

						diaEvent.type = Dia::Input::Event::EType::kJoystickMoved;
						diaEvent.joystickMove.joystickId = sfInput->joystickId;
						diaEvent.joystickMove.axis = static_cast<int>(sfInput->axis);
						diaEvent.joystickMove.position = sfInput->position;
					}
					else if (sfEvent->is<sf::Event::JoystickButtonPressed>() || sfEvent->is<sf::Event::JoystickButtonReleased>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::JoystickButtonPressed>();

						if (sfEvent->is<sf::Event::JoystickConnected>())
							diaEvent.type = Dia::Input::Event::EType::kJoystickButtonPressed;
						else
							diaEvent.type = Dia::Input::Event::EType::kJoystickButtonReleased;

						diaEvent.joystickButton.joystickId = sfInput->joystickId;
						diaEvent.joystickButton.button = sfInput->button;
					}
					else if (sfEvent->is<sf::Event::Closed>())
					{
						const auto* sfInput = sfEvent->getIf<sf::Event::Closed>();

						diaEvent.type = Dia::Input::Event::EType::kClosed;
					}
					else
					{
						// Unhandled event type
					//	DIA_ASSERT(nullptr, "Unhandled event type in InputSource::Poll");
						continue; // Skip unhandled events
					}

					// Add event to output stream if listening for this type
					if (IsListeningForEvent(diaEvent.type))
					{
						// Merge consecutive MouseMoved events for efficiency
						if (sfEvent->is<sf::Event::MouseMoved>() &&
							outStream.Size() > 0 &&
							outStream[outStream.Size() - 1].type == Dia::Input::Event::EType::kMouseMoved)
						{
							outStream[outStream.Size() - 1].mouseMove.x = diaEvent.mouseMove.x;
							outStream[outStream.Size() - 1].mouseMove.y = diaEvent.mouseMove.y;
						}
						else
						{
							outStream.Add(diaEvent);
						}
					}
				}
			}
		}

		//------------------------------------------------------
		// Checks if the current input source is enabled for the given event type.
		// Returns true if the event type is enabled in the listening mask.
		bool InputSource::IsListeningForEvent(Dia::Input::Event::EType eventType)const
		{
			bool isListening = false;

			switch (eventType)
			{
			case Dia::Input::Event::EType::kClosed:
			case Dia::Input::Event::EType::kResized:
			case Dia::Input::Event::EType::kLostFocus:
			case Dia::Input::Event::EType::kGainedFocus:
				isListening = mListeningToSource[ESourceIndex::kSystem];
				break;
			case Dia::Input::Event::EType::kKeyPressed:
			case Dia::Input::Event::EType::kKeyReleased:
			case Dia::Input::Event::EType::kTextEntered:
				isListening = mListeningToSource[ESourceIndex::kKeyboard];
				break;
			case Dia::Input::Event::EType::kMouseMoved:
			case Dia::Input::Event::EType::kMouseButtonPressed:
			case Dia::Input::Event::EType::kMouseButtonReleased:
				isListening = mListeningToSource[ESourceIndex::kMouse];
				break;
			case Dia::Input::Event::EType::kJoystickConnected:
			case Dia::Input::Event::EType::kJoystickDisconnected:
			case Dia::Input::Event::EType::kJoystickMoved:
			case Dia::Input::Event::EType::kJoystickButtonPressed:
			case Dia::Input::Event::EType::kJoystickButtonReleased:
				isListening = mListeningToSource[ESourceIndex::kJoystick];
				break;
			default:
				break;
			}
			return isListening;
		}
	}
}