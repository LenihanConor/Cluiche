////////////////////////////////////////////////////////////////////////////////
// Filename: InputSource.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaInput/IInputSource.h>
#include <DiaCore/Containers/BitFlag/BitArray8.h>
#include <DiaCore/Core/EnumClass.h>
#include <DiaInput/Event.h>

struct SDL_Window;
union SDL_Event;
typedef unsigned int SDL_Keycode;

namespace Dia
{
	namespace SDL
	{
		class InputSource : public Dia::Input::IInputSource
		{
		public:
			CLASSEDENUM(ESourceIndex, \
				CE_ITEMVAL(kSystem,   0)\
				CE_ITEMVAL(kKeyboard, 1)\
				CE_ITEMVAL(kMouse,    3)\
				CE_ITEMVAL(kJoystick, 4)\
				, kSystem \
				);

			CLASSEDENUM(ESources, \
				CE_ITEMVAL(kSystem,   1 << ESourceIndex::kSystem)\
				CE_ITEMVAL(kKeyboard, 1 << ESourceIndex::kKeyboard)\
				CE_ITEMVAL(kMouse,    1 << ESourceIndex::kMouse)\
				CE_ITEMVAL(kJoystick, 1 << ESourceIndex::kJoystick)\
				, kSystem \
				);

			InputSource();

			void SetWindowContext(SDL_Window* window);
			void ListenForInputSources(Dia::Core::BitArray8 listeningToSource) override;
			void Poll(Dia::Input::EventData& outStream) override;

			// Public statics for direct unit testing (T-06b)
			static Dia::Input::EKey SDLKeycodeToEKey(SDL_Keycode key);
			static bool TranslateSDLEvent(const SDL_Event& sdlEvent, Dia::Input::Event& outEvent);

		protected:
			// Called for each SDL event during Poll(). Override to forward events
			// to subsystems (e.g. ImGui). Default implementation is a no-op.
			virtual void OnRawSDLEvent(const SDL_Event& /*event*/) {}

		private:
			bool IsListeningForEvent(Dia::Input::Event::EType eventType) const;

			SDL_Window*             mWindowContext;
			Dia::Core::BitArray8    mListeningToSource;
		};
	}
}
