////////////////////////////////////////////////////////////////////////////////
// Filename: InputSource.cpp
////////////////////////////////////////////////////////////////////////////////

#include "DiaSDL/InputSource.h"

#include <SDL3/SDL.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Trace/TraceCategory.h>
#include <DiaInput/EventData.h>
#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace SDL
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
		// Sets the SDL window context to poll events from.
		// This must be called before polling for input.
		void InputSource::SetWindowContext(SDL_Window* window)
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
		// Polls the SDL window for events, converts them to Dia::Input::Event,
		// and adds them to the output stream if the event type is enabled.
		// MouseMoved events are merged if consecutive.
		void InputSource::Poll(Dia::Input::EventData& outStream)
		{
			DIA_TRACE_ZONE("InputSource::Poll", Dia::Observation::Trace::Category::kDiaApplicationFlow);
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				outStream.RemoveAll();

				SDL_Event sdlEvent;
				while (SDL_PollEvent(&sdlEvent))
				{
					// Forward raw event to subsystems (e.g. ImGui backend)
					OnRawSDLEvent(sdlEvent);

					Dia::Input::Event diaEvent;

					if (!TranslateSDLEvent(sdlEvent, diaEvent))
					{
						// Unhandled event type — skip
						continue;
					}

					// Add event to output stream if listening for this type
					if (IsListeningForEvent(diaEvent.type))
					{
						// Merge consecutive MouseMoved events for efficiency
						if (sdlEvent.type == SDL_EVENT_MOUSE_MOTION &&
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
		// Maps an SDL_Keycode to a Dia::Input::EKey.
		// Returns EKey::Unknown for unrecognised codes (with a debug log).
		Dia::Input::EKey InputSource::SDLKeycodeToEKey(SDL_Keycode key)
		{
			switch (key)
			{
			// Letters
			case SDLK_A: return Dia::Input::EKey::A;
			case SDLK_B: return Dia::Input::EKey::B;
			case SDLK_C: return Dia::Input::EKey::C;
			case SDLK_D: return Dia::Input::EKey::D;
			case SDLK_E: return Dia::Input::EKey::E;
			case SDLK_F: return Dia::Input::EKey::F;
			case SDLK_G: return Dia::Input::EKey::G;
			case SDLK_H: return Dia::Input::EKey::H;
			case SDLK_I: return Dia::Input::EKey::I;
			case SDLK_J: return Dia::Input::EKey::J;
			case SDLK_K: return Dia::Input::EKey::K;
			case SDLK_L: return Dia::Input::EKey::L;
			case SDLK_M: return Dia::Input::EKey::M;
			case SDLK_N: return Dia::Input::EKey::N;
			case SDLK_O: return Dia::Input::EKey::O;
			case SDLK_P: return Dia::Input::EKey::P;
			case SDLK_Q: return Dia::Input::EKey::Q;
			case SDLK_R: return Dia::Input::EKey::R;
			case SDLK_S: return Dia::Input::EKey::S;
			case SDLK_T: return Dia::Input::EKey::T;
			case SDLK_U: return Dia::Input::EKey::U;
			case SDLK_V: return Dia::Input::EKey::V;
			case SDLK_W: return Dia::Input::EKey::W;
			case SDLK_X: return Dia::Input::EKey::X;
			case SDLK_Y: return Dia::Input::EKey::Y;
			case SDLK_Z: return Dia::Input::EKey::Z;

			// Number row
			case SDLK_0: return Dia::Input::EKey::Num0;
			case SDLK_1: return Dia::Input::EKey::Num1;
			case SDLK_2: return Dia::Input::EKey::Num2;
			case SDLK_3: return Dia::Input::EKey::Num3;
			case SDLK_4: return Dia::Input::EKey::Num4;
			case SDLK_5: return Dia::Input::EKey::Num5;
			case SDLK_6: return Dia::Input::EKey::Num6;
			case SDLK_7: return Dia::Input::EKey::Num7;
			case SDLK_8: return Dia::Input::EKey::Num8;
			case SDLK_9: return Dia::Input::EKey::Num9;

			// Control keys
			case SDLK_ESCAPE:    return Dia::Input::EKey::Escape;
			case SDLK_LCTRL:     return Dia::Input::EKey::LControl;
			case SDLK_RCTRL:     return Dia::Input::EKey::RControl;
			case SDLK_LSHIFT:    return Dia::Input::EKey::LShift;
			case SDLK_RSHIFT:    return Dia::Input::EKey::RShift;
			case SDLK_LALT:      return Dia::Input::EKey::LAlt;
			case SDLK_RALT:      return Dia::Input::EKey::RAlt;
			case SDLK_LGUI:      return Dia::Input::EKey::LSystem;
			case SDLK_RGUI:      return Dia::Input::EKey::RSystem;
			case SDLK_MENU:      return Dia::Input::EKey::Menu;

			// Punctuation / symbols
			case SDLK_LEFTBRACKET:  return Dia::Input::EKey::LBracket;
			case SDLK_RIGHTBRACKET: return Dia::Input::EKey::RBracket;
			case SDLK_SEMICOLON:    return Dia::Input::EKey::SemiColon;
			case SDLK_COMMA:        return Dia::Input::EKey::Comma;
			case SDLK_PERIOD:       return Dia::Input::EKey::Period;
			case SDLK_APOSTROPHE:   return Dia::Input::EKey::Quote;
			case SDLK_SLASH:        return Dia::Input::EKey::Slash;
			case SDLK_BACKSLASH:    return Dia::Input::EKey::BackSlash;
			case SDLK_GRAVE:        return Dia::Input::EKey::Tilde;
			case SDLK_EQUALS:       return Dia::Input::EKey::Equal;
			case SDLK_MINUS:        return Dia::Input::EKey::Dash;

			// Whitespace / editing
			case SDLK_SPACE:     return Dia::Input::EKey::Space;
			case SDLK_RETURN:    return Dia::Input::EKey::Return;
			case SDLK_BACKSPACE: return Dia::Input::EKey::BackSpace;
			case SDLK_TAB:       return Dia::Input::EKey::Tab;

			// Navigation
			case SDLK_PAGEUP:    return Dia::Input::EKey::PageUp;
			case SDLK_PAGEDOWN:  return Dia::Input::EKey::PageDown;
			case SDLK_END:       return Dia::Input::EKey::End;
			case SDLK_HOME:      return Dia::Input::EKey::Home;
			case SDLK_INSERT:    return Dia::Input::EKey::Insert;
			case SDLK_DELETE:    return Dia::Input::EKey::Delete;

			// Numpad arithmetic
			case SDLK_KP_PLUS:     return Dia::Input::EKey::Add;
			case SDLK_KP_MINUS:    return Dia::Input::EKey::Subtract;
			case SDLK_KP_MULTIPLY: return Dia::Input::EKey::Multiply;
			case SDLK_KP_DIVIDE:   return Dia::Input::EKey::Divide;

			// Arrow keys
			case SDLK_LEFT:  return Dia::Input::EKey::Left;
			case SDLK_RIGHT: return Dia::Input::EKey::Right;
			case SDLK_UP:    return Dia::Input::EKey::Up;
			case SDLK_DOWN:  return Dia::Input::EKey::Down;

			// Numpad digits
			case SDLK_KP_0: return Dia::Input::EKey::Numpad0;
			case SDLK_KP_1: return Dia::Input::EKey::Numpad1;
			case SDLK_KP_2: return Dia::Input::EKey::Numpad2;
			case SDLK_KP_3: return Dia::Input::EKey::Numpad3;
			case SDLK_KP_4: return Dia::Input::EKey::Numpad4;
			case SDLK_KP_5: return Dia::Input::EKey::Numpad5;
			case SDLK_KP_6: return Dia::Input::EKey::Numpad6;
			case SDLK_KP_7: return Dia::Input::EKey::Numpad7;
			case SDLK_KP_8: return Dia::Input::EKey::Numpad8;
			case SDLK_KP_9: return Dia::Input::EKey::Numpad9;

			// Function keys
			case SDLK_F1:  return Dia::Input::EKey::F1;
			case SDLK_F2:  return Dia::Input::EKey::F2;
			case SDLK_F3:  return Dia::Input::EKey::F3;
			case SDLK_F4:  return Dia::Input::EKey::F4;
			case SDLK_F5:  return Dia::Input::EKey::F5;
			case SDLK_F6:  return Dia::Input::EKey::F6;
			case SDLK_F7:  return Dia::Input::EKey::F7;
			case SDLK_F8:  return Dia::Input::EKey::F8;
			case SDLK_F9:  return Dia::Input::EKey::F9;
			case SDLK_F10: return Dia::Input::EKey::F10;
			case SDLK_F11: return Dia::Input::EKey::F11;
			case SDLK_F12: return Dia::Input::EKey::F12;

			// Misc
			case SDLK_PAUSE: return Dia::Input::EKey::Pause;

			default:
				DIA_LOG_DEBUG("DiaSDL", "SDLKeycodeToEKey: unrecognised SDL_Keycode %d", static_cast<int>(key));
				return Dia::Input::EKey::Unknown;
			}
		}

		//------------------------------------------------------
		// Translates an SDL_Event into a Dia::Input::Event.
		// Returns false if the event type is not handled (caller should skip it).
		bool InputSource::TranslateSDLEvent(const SDL_Event& sdlEvent, Dia::Input::Event& outEvent)
		{
			switch (sdlEvent.type)
			{
			case SDL_EVENT_QUIT:
			{
				outEvent.type = Dia::Input::Event::EType::kClosed;
				return true;
			}

			case SDL_EVENT_WINDOW_RESIZED:
			{
				outEvent.type        = Dia::Input::Event::EType::kResized;
				outEvent.size.width  = static_cast<unsigned int>(sdlEvent.window.data1);
				outEvent.size.height = static_cast<unsigned int>(sdlEvent.window.data2);
				return true;
			}

			case SDL_EVENT_KEY_DOWN:
			{
				outEvent.type         = Dia::Input::Event::EType::kKeyPressed;
				outEvent.key.code     = static_cast<int>(SDLKeycodeToEKey(sdlEvent.key.key));
				outEvent.key.shift    = (sdlEvent.key.mod & SDL_KMOD_SHIFT)   != 0;
				outEvent.key.control  = (sdlEvent.key.mod & SDL_KMOD_CTRL)    != 0;
				outEvent.key.alt      = (sdlEvent.key.mod & SDL_KMOD_ALT)     != 0;
				outEvent.key.system   = (sdlEvent.key.mod & SDL_KMOD_GUI)     != 0;
				return true;
			}

			case SDL_EVENT_KEY_UP:
			{
				outEvent.type         = Dia::Input::Event::EType::kKeyReleased;
				outEvent.key.code     = static_cast<int>(SDLKeycodeToEKey(sdlEvent.key.key));
				outEvent.key.shift    = (sdlEvent.key.mod & SDL_KMOD_SHIFT)   != 0;
				outEvent.key.control  = (sdlEvent.key.mod & SDL_KMOD_CTRL)    != 0;
				outEvent.key.alt      = (sdlEvent.key.mod & SDL_KMOD_ALT)     != 0;
				outEvent.key.system   = (sdlEvent.key.mod & SDL_KMOD_GUI)     != 0;
				return true;
			}

			case SDL_EVENT_TEXT_INPUT:
			{
				outEvent.type          = Dia::Input::Event::EType::kTextEntered;
				// UTF-8 text — use the first byte cast to unsigned int
				outEvent.text.unicode  = static_cast<unsigned int>(
					static_cast<unsigned char>(sdlEvent.text.text[0]));
				return true;
			}

			case SDL_EVENT_MOUSE_MOTION:
			{
				outEvent.type            = Dia::Input::Event::EType::kMouseMoved;
				outEvent.mouseMove.x     = static_cast<int>(sdlEvent.motion.x);
				outEvent.mouseMove.y     = static_cast<int>(sdlEvent.motion.y);
				return true;
			}

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			{
				outEvent.type              = Dia::Input::Event::EType::kMouseButtonPressed;
				// SDL_BUTTON_LEFT=1, SDL_BUTTON_RIGHT=3, SDL_BUTTON_MIDDLE=2
				// Map to Dia convention: Left=0, Right=1, Middle=2
				switch (sdlEvent.button.button)
				{
				case SDL_BUTTON_LEFT:   outEvent.mouseButton.button = 0; break;
				case SDL_BUTTON_RIGHT:  outEvent.mouseButton.button = 1; break;
				case SDL_BUTTON_MIDDLE: outEvent.mouseButton.button = 2; break;
				default:                outEvent.mouseButton.button = static_cast<int>(sdlEvent.button.button); break;
				}
				outEvent.mouseButton.x = static_cast<int>(sdlEvent.button.x);
				outEvent.mouseButton.y = static_cast<int>(sdlEvent.button.y);
				return true;
			}

			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				outEvent.type              = Dia::Input::Event::EType::kMouseButtonReleased;
				switch (sdlEvent.button.button)
				{
				case SDL_BUTTON_LEFT:   outEvent.mouseButton.button = 0; break;
				case SDL_BUTTON_RIGHT:  outEvent.mouseButton.button = 1; break;
				case SDL_BUTTON_MIDDLE: outEvent.mouseButton.button = 2; break;
				default:                outEvent.mouseButton.button = static_cast<int>(sdlEvent.button.button); break;
				}
				outEvent.mouseButton.x = static_cast<int>(sdlEvent.button.x);
				outEvent.mouseButton.y = static_cast<int>(sdlEvent.button.y);
				return true;
			}

			case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			{
				outEvent.type                     = Dia::Input::Event::EType::kJoystickButtonPressed;
				outEvent.joystickButton.joystickId = static_cast<unsigned int>(sdlEvent.jbutton.which);
				outEvent.joystickButton.button     = sdlEvent.jbutton.button;
				return true;
			}

			case SDL_EVENT_JOYSTICK_BUTTON_UP:
			{
				outEvent.type                     = Dia::Input::Event::EType::kJoystickButtonReleased;
				outEvent.joystickButton.joystickId = static_cast<unsigned int>(sdlEvent.jbutton.which);
				outEvent.joystickButton.button     = sdlEvent.jbutton.button;
				return true;
			}

			case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			{
				outEvent.type                   = Dia::Input::Event::EType::kJoystickMoved;
				outEvent.joystickMove.joystickId = static_cast<unsigned int>(sdlEvent.jaxis.which);
				outEvent.joystickMove.axis       = static_cast<int>(sdlEvent.jaxis.axis);
				// Normalise Sint16 range [-32768..32767] → [-100..100]
				outEvent.joystickMove.position   = sdlEvent.jaxis.value / 32767.0f * 100.0f;
				return true;
			}

			case SDL_EVENT_JOYSTICK_ADDED:
			{
				outEvent.type                        = Dia::Input::Event::EType::kJoystickConnected;
				outEvent.joystickConnect.joystickId  = static_cast<unsigned int>(sdlEvent.jdevice.which);
				return true;
			}

			case SDL_EVENT_JOYSTICK_REMOVED:
			{
				outEvent.type                        = Dia::Input::Event::EType::kJoystickDisconnected;
				outEvent.joystickConnect.joystickId  = static_cast<unsigned int>(sdlEvent.jdevice.which);
				return true;
			}

			default:
				return false;
			}
		}

		//------------------------------------------------------
		// Checks if the current input source is enabled for the given event type.
		// Returns true if the event type is enabled in the listening mask.
		bool InputSource::IsListeningForEvent(Dia::Input::Event::EType eventType) const
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
