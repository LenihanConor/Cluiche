#ifndef DIA_INPUT_KEYBOARD_EVENTS_H
#define DIA_INPUT_KEYBOARD_EVENTS_H

#include <DiaCore/Architecture/Events/Event.h>
#include "DiaInput/EKey.h"

namespace Dia
{
	namespace Input
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// KeyPressedEvent
			//
			// Fired when a keyboard key is pressed down.
			//---------------------------------------------------------------------------------------------------------------------------------
			class KeyPressedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(KeyPressedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Keyboard)

			public:
				KeyPressedEvent(EKey key, bool alt, bool ctrl, bool shift, bool system)
					: mKey(key), mAlt(alt), mControl(ctrl), mShift(shift), mSystem(system) {}

				EKey GetKey() const { return mKey; }
				bool IsAlt() const { return mAlt; }
				bool IsControl() const { return mControl; }
				bool IsShift() const { return mShift; }
				bool IsSystem() const { return mSystem; }

				virtual Core::Events::Event* Clone() const override
				{
					return new KeyPressedEvent(mKey, mAlt, mControl, mShift, mSystem);
				}

			private:
				EKey mKey;
				bool mAlt;
				bool mControl;
				bool mShift;
				bool mSystem;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// KeyReleasedEvent
			//
			// Fired when a keyboard key is released.
			//---------------------------------------------------------------------------------------------------------------------------------
			class KeyReleasedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(KeyReleasedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Keyboard)

			public:
				KeyReleasedEvent(EKey key, bool alt, bool ctrl, bool shift, bool system)
					: mKey(key), mAlt(alt), mControl(ctrl), mShift(shift), mSystem(system) {}

				EKey GetKey() const { return mKey; }
				bool IsAlt() const { return mAlt; }
				bool IsControl() const { return mControl; }
				bool IsShift() const { return mShift; }
				bool IsSystem() const { return mSystem; }

				virtual Core::Events::Event* Clone() const override
				{
					return new KeyReleasedEvent(mKey, mAlt, mControl, mShift, mSystem);
				}

			private:
				EKey mKey;
				bool mAlt;
				bool mControl;
				bool mShift;
				bool mSystem;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// TextEnteredEvent
			//
			// Fired when a text character is entered (Unicode).
			//---------------------------------------------------------------------------------------------------------------------------------
			class TextEnteredEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(TextEnteredEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Keyboard)

			public:
				TextEnteredEvent(unsigned int unicode)
					: mUnicode(unicode) {}

				unsigned int GetUnicode() const { return mUnicode; }

				virtual Core::Events::Event* Clone() const override
				{
					return new TextEnteredEvent(mUnicode);
				}

			private:
				unsigned int mUnicode;
			};
		}
	}
}

#endif // DIA_INPUT_KEYBOARD_EVENTS_H
