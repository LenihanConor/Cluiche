#ifndef DIA_INPUT_MOUSE_EVENTS_H
#define DIA_INPUT_MOUSE_EVENTS_H

#include <DiaCore/Architecture/Events/Event.h>
#include "DiaInput/EMouseButton.h"

namespace Dia
{
	namespace Input
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// MouseButtonPressedEvent
			//
			// Fired when a mouse button is pressed.
			//---------------------------------------------------------------------------------------------------------------------------------
			class MouseButtonPressedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(MouseButtonPressedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Mouse | Core::Events::EventCategory_MouseButton)

			public:
				MouseButtonPressedEvent(EMouseButton button, int x, int y)
					: mButton(button), mX(x), mY(y) {}

				EMouseButton GetButton() const { return mButton; }
				int GetX() const { return mX; }
				int GetY() const { return mY; }

				virtual Core::Events::Event* Clone() const override
				{
					return new MouseButtonPressedEvent(mButton, mX, mY);
				}

			private:
				EMouseButton mButton;
				int mX;
				int mY;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// MouseButtonReleasedEvent
			//
			// Fired when a mouse button is released.
			//---------------------------------------------------------------------------------------------------------------------------------
			class MouseButtonReleasedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(MouseButtonReleasedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Mouse | Core::Events::EventCategory_MouseButton)

			public:
				MouseButtonReleasedEvent(EMouseButton button, int x, int y)
					: mButton(button), mX(x), mY(y) {}

				EMouseButton GetButton() const { return mButton; }
				int GetX() const { return mX; }
				int GetY() const { return mY; }

				virtual Core::Events::Event* Clone() const override
				{
					return new MouseButtonReleasedEvent(mButton, mX, mY);
				}

			private:
				EMouseButton mButton;
				int mX;
				int mY;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// MouseMovedEvent
			//
			// Fired when the mouse cursor moves.
			//---------------------------------------------------------------------------------------------------------------------------------
			class MouseMovedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(MouseMovedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Mouse)

			public:
				MouseMovedEvent(int x, int y)
					: mX(x), mY(y) {}

				int GetX() const { return mX; }
				int GetY() const { return mY; }

				virtual Core::Events::Event* Clone() const override
				{
					return new MouseMovedEvent(mX, mY);
				}

			private:
				int mX;
				int mY;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// MouseWheelMovedEvent
			//
			// Fired when the mouse wheel is scrolled.
			//---------------------------------------------------------------------------------------------------------------------------------
			class MouseWheelMovedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(MouseWheelMovedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Mouse)

			public:
				MouseWheelMovedEvent(int delta, int x, int y)
					: mDelta(delta), mX(x), mY(y) {}

				int GetDelta() const { return mDelta; }
				int GetX() const { return mX; }
				int GetY() const { return mY; }

				virtual Core::Events::Event* Clone() const override
				{
					return new MouseWheelMovedEvent(mDelta, mX, mY);
				}

			private:
				int mDelta;
				int mX;
				int mY;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// MouseEnteredEvent
			//
			// Fired when the mouse cursor enters the window.
			//---------------------------------------------------------------------------------------------------------------------------------
			class MouseEnteredEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(MouseEnteredEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Mouse)

			public:
				MouseEnteredEvent() {}

				virtual Core::Events::Event* Clone() const override
				{
					return new MouseEnteredEvent();
				}
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// MouseLeftEvent
			//
			// Fired when the mouse cursor leaves the window.
			//---------------------------------------------------------------------------------------------------------------------------------
			class MouseLeftEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(MouseLeftEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input | Core::Events::EventCategory_Mouse)

			public:
				MouseLeftEvent() {}

				virtual Core::Events::Event* Clone() const override
				{
					return new MouseLeftEvent();
				}
			};
		}
	}
}

#endif // DIA_INPUT_MOUSE_EVENTS_H
