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
		InputSource::InputSource()
			: mWindowContext(nullptr)
		{
			// Default to listening to everything
			mListeningToSource.SetAllBits(0xFF);
		}

		//------------------------------------------------------
		void InputSource::SetWindowContext(sf::Window* window)
		{
			mWindowContext = window;
		}

		//------------------------------------------------------
		void InputSource::ListenForInputSources(Dia::Core::BitArray8 listeningToSource)
		{
			mListeningToSource = listeningToSource;
		}

		//------------------------------------------------------
		void InputSource::Poll(Dia::Input::EventData& outStream)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				outStream.RemoveAll();

				// handle events
				sf::Event sfEvent;
				while (mWindowContext->pollEvent(sfEvent))
				{
					Dia::Input::Event diaEvent;

					Convert(diaEvent, sfEvent);

					if (IsListeningForEvent(diaEvent.type))
					{
						outStream.Add(diaEvent);
					}
				}
			}
		}

		//------------------------------------------------------
		void InputSource::Convert(Dia::Input::Event& lhs, const sf::Event& rhs)const
		{
			bool isInTypeRange = (rhs.type >= 0 && rhs.type < Dia::Input::Event::EType::NumberOfItems);

			if (!isInTypeRange)
			{
				DIA_ASSERT(0, "Cannot convert sf event into dia event as it is outside range of types");
			}

			lhs.type = rhs.type;

			switch (rhs.type)
			{
			case sf::Event::Resized:
				lhs.size.height = rhs.size.height;
				lhs.size.width = rhs.size.width;
				break;
			case sf::Event::KeyPressed:
			case sf::Event::KeyReleased:
				lhs.key.code = rhs.key.code;    
				lhs.key.alt = rhs.key.alt;
				lhs.key.control = rhs.key.control;
				lhs.key.shift = rhs.key.shift;
				lhs.key.system = rhs.key.system;
				break;
			case sf::Event::TextEntered:
				lhs.text.unicode = rhs.text.unicode;
				break;
			case sf::Event::MouseMoved:
				lhs.mouseMove.x = rhs.mouseMove.x;
				lhs.mouseMove.y = rhs.mouseMove.y;
				break;
			case sf::Event::MouseButtonPressed:
			case sf::Event::MouseButtonReleased:
				lhs.mouseButton.button = rhs.mouseButton.button;
				lhs.mouseButton.x = rhs.mouseButton.x;
				lhs.mouseButton.y = rhs.mouseButton.y;
				break;
			case sf::Event::JoystickConnected:
			case sf::Event::JoystickDisconnected:
				lhs.joystickConnect.joystickId = rhs.joystickConnect.joystickId;
				break;
			case sf::Event::JoystickMoved:
				lhs.joystickMove.joystickId = rhs.joystickMove.joystickId;
				lhs.joystickMove.axis = rhs.joystickMove.axis;
				lhs.joystickMove.position = rhs.joystickMove.position;
				break;
			case sf::Event::JoystickButtonPressed:
			case sf::Event::JoystickButtonReleased:
				lhs.joystickButton.joystickId = rhs.joystickButton.joystickId;
				lhs.joystickButton.button = rhs.joystickButton.button;
				break;
			default:
				break;
			}
		}

		//------------------------------------------------------
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
			case sf::Event::KeyPressed:
			case sf::Event::KeyReleased:
			case sf::Event::TextEntered:
				isListening = mListeningToSource[ESourceIndex::kKeyboard];
				break;
			case sf::Event::MouseMoved:
			case sf::Event::MouseButtonPressed:
			case sf::Event::MouseButtonReleased:
				isListening = mListeningToSource[ESourceIndex::kMouse];
				break;
			case sf::Event::JoystickConnected:
			case sf::Event::JoystickDisconnected:
			case sf::Event::JoystickMoved:
			case sf::Event::JoystickButtonPressed:
			case sf::Event::JoystickButtonReleased:
				isListening = mListeningToSource[ESourceIndex::kJoystick];
				break;
			default:
				break;
			}
			return isListening;
		}
	}
}