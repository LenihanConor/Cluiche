////////////////////////////////////////////////////////////////////////////////
// Filename: InputSource.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaInput\IInputSource.h>
#include <DiaCore/Containers/BitFlag/BitArray8.h>

namespace sf
{
	class Window;
	class Event;
}

namespace Dia
{
	namespace Input
	{
		class Event;
	}
}

namespace Dia
{
	namespace SFML
	{
		class InputSource: public Dia::Input::IInputSource
		{
		public:	
			////////////////////////////////////////////////////////////////////////////////
			// Enum name: ESourcesIndex - used internally
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM(ESourceIndex, \
				CE_ITEMVAL(kSystem, 0)\
				CE_ITEMVAL(kKeyboard, 1)\
				CE_ITEMVAL(kMouse, 3)\
				CE_ITEMVAL(kJoystick, 4)\
				, kSystem \
				);

			////////////////////////////////////////////////////////////////////////////////
			// Enum name: ESources - use this to concate sources together
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM(ESources, \
				CE_ITEMVAL(kSystem, 1 << ESourceIndex::kSystem)\
				CE_ITEMVAL(kKeyboard, 1 << ESourceIndex::kKeyboard)\
				CE_ITEMVAL(kMouse, 1 << ESourceIndex::kMouse)\
				CE_ITEMVAL(kJoystick, 1 << ESourceIndex::kJoystick)\
				, kSystem \
				);

			InputSource();
			
			void SetWindowContext(sf::Window* window);

			void ListenForInputSources(Dia::Core::BitArray8 listeningToSource);

			virtual void Poll(Dia::Input::EventStream& outStream)override;

		private:
			void Convert(Dia::Input::Event& lhs, const sf::Event& rhs)const;
			
			bool IsListeningForEvent(Dia::Input::Event::EType eventType)const;

			sf::Window* mWindowContext;
			Dia::Core::BitArray8 mListeningToSource;
		};
	}
}