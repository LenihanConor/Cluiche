#ifndef DIA_INPUT_JOYSTICK_EVENTS_H
#define DIA_INPUT_JOYSTICK_EVENTS_H

#include <DiaCore/Architecture/Events/Event.h>
#include "DiaInput/EJoystick.h"

namespace Dia
{
	namespace Input
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// JoystickButtonPressedEvent
			//
			// Fired when a joystick button is pressed.
			//---------------------------------------------------------------------------------------------------------------------------------
			class JoystickButtonPressedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(JoystickButtonPressedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				JoystickButtonPressedEvent(unsigned int joystickId, unsigned int button)
					: mJoystickId(joystickId), mButton(button) {}

				unsigned int GetJoystickId() const { return mJoystickId; }
				unsigned int GetButton() const { return mButton; }

				virtual Core::Events::Event* Clone() const override
				{
					return new JoystickButtonPressedEvent(mJoystickId, mButton);
				}

			private:
				unsigned int mJoystickId;
				unsigned int mButton;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// JoystickButtonReleasedEvent
			//
			// Fired when a joystick button is released.
			//---------------------------------------------------------------------------------------------------------------------------------
			class JoystickButtonReleasedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(JoystickButtonReleasedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				JoystickButtonReleasedEvent(unsigned int joystickId, unsigned int button)
					: mJoystickId(joystickId), mButton(button) {}

				unsigned int GetJoystickId() const { return mJoystickId; }
				unsigned int GetButton() const { return mButton; }

				virtual Core::Events::Event* Clone() const override
				{
					return new JoystickButtonReleasedEvent(mJoystickId, mButton);
				}

			private:
				unsigned int mJoystickId;
				unsigned int mButton;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// JoystickAxisMovedEvent
			//
			// Fired when a joystick axis moves.
			//---------------------------------------------------------------------------------------------------------------------------------
			class JoystickAxisMovedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(JoystickAxisMovedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				JoystickAxisMovedEvent(unsigned int joystickId, EJoystickAxis axis, float position)
					: mJoystickId(joystickId), mAxis(axis), mPosition(position) {}

				unsigned int GetJoystickId() const { return mJoystickId; }
				EJoystickAxis GetAxis() const { return mAxis; }
				float GetPosition() const { return mPosition; }

				virtual Core::Events::Event* Clone() const override
				{
					return new JoystickAxisMovedEvent(mJoystickId, mAxis, mPosition);
				}

			private:
				unsigned int mJoystickId;
				EJoystickAxis mAxis;
				float mPosition;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// JoystickConnectedEvent
			//
			// Fired when a joystick is connected (hot-plugged).
			//---------------------------------------------------------------------------------------------------------------------------------
			class JoystickConnectedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(JoystickConnectedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				JoystickConnectedEvent(unsigned int joystickId)
					: mJoystickId(joystickId) {}

				unsigned int GetJoystickId() const { return mJoystickId; }

				virtual Core::Events::Event* Clone() const override
				{
					return new JoystickConnectedEvent(mJoystickId);
				}

			private:
				unsigned int mJoystickId;
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// JoystickDisconnectedEvent
			//
			// Fired when a joystick is disconnected (unplugged).
			//---------------------------------------------------------------------------------------------------------------------------------
			class JoystickDisconnectedEvent : public Core::Events::Event
			{
				EVENT_CLASS_TYPE(JoystickDisconnectedEvent)
				EVENT_CLASS_CATEGORY(Core::Events::EventCategory_Input)

			public:
				JoystickDisconnectedEvent(unsigned int joystickId)
					: mJoystickId(joystickId) {}

				unsigned int GetJoystickId() const { return mJoystickId; }

				virtual Core::Events::Event* Clone() const override
				{
					return new JoystickDisconnectedEvent(mJoystickId);
				}

			private:
				unsigned int mJoystickId;
			};
		}
	}
}

#endif // DIA_INPUT_JOYSTICK_EVENTS_H
