#ifndef DIA_EVENT_H
#define DIA_EVENT_H

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Time/TimeAbsolute.h"

namespace Dia
{
	namespace Core
	{
		namespace Events
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Event
			//
			// Base class for all events in the system.
			// Events carry data and can be queued, dispatched, and handled.
			//
			// USAGE:
			//   class MyEvent : public Event {
			//       EVENT_CLASS_TYPE(MyEvent)
			//   public:
			//       int data;
			//   };
			//
			// FEATURES:
			//   - Type identification via CRC
			//   - Timestamp for event ordering
			//   - Handled flag for event propagation control
			//   - Clone support for event copying
			//---------------------------------------------------------------------------------------------------------------------------------

			using EventTypeID = StringCRC;

			//-----------------------------------------------------------------------------
			// Event Category Flags (bitfield for filtering)
			//-----------------------------------------------------------------------------
			enum EventCategory
			{
				EventCategory_None        = 0,
				EventCategory_Application = 1 << 0,
				EventCategory_Input       = 1 << 1,
				EventCategory_Keyboard    = 1 << 2,
				EventCategory_Mouse       = 1 << 3,
				EventCategory_MouseButton = 1 << 4,
				EventCategory_Gameplay    = 1 << 5,
				EventCategory_UI          = 1 << 6,
				EventCategory_Network     = 1 << 7,
				EventCategory_Audio       = 1 << 8,
				EventCategory_Custom      = 1 << 9
			};

			//-----------------------------------------------------------------------------
			// Event Base Class
			//-----------------------------------------------------------------------------
			class Event
			{
			public:
				Event()
					: mHandled(false)
					, mTimestamp()
				{}

				virtual ~Event() {}

				// Get event type
				virtual EventTypeID GetEventType() const = 0;
				virtual const char* GetName() const = 0;
				virtual int GetCategoryFlags() const { return EventCategory_None; }

				// Check if event is in category
				bool IsInCategory(EventCategory category) const
				{
					return (GetCategoryFlags() & category) != 0;
				}

				// Event handling state
				bool IsHandled() const { return mHandled; }
				void SetHandled(bool handled) { mHandled = handled; }

				// Timestamp
				const TimeAbsolute& GetTimestamp() const { return mTimestamp; }
				void SetTimestamp(const TimeAbsolute& time) { mTimestamp = time; }

				// Clone event (for copying into queues)
				virtual Event* Clone() const = 0;

			protected:
				bool mHandled;
				TimeAbsolute mTimestamp;
			};

			//-----------------------------------------------------------------------------
			// Event Class Type Macro
			// Use this in derived event classes to implement type identification
			//-----------------------------------------------------------------------------
			#define EVENT_CLASS_TYPE(type) \
				static EventTypeID GetStaticType() { static StringCRC id(#type); return id; } \
				virtual EventTypeID GetEventType() const override { return GetStaticType(); } \
				virtual const char* GetName() const override { return #type; } \
				virtual Event* Clone() const override { return new type(*this); }

			#define EVENT_CLASS_CATEGORY(category) \
				virtual int GetCategoryFlags() const override { return category; }

			//-----------------------------------------------------------------------------
			// Event Casting
			//-----------------------------------------------------------------------------
			template <typename T>
			T* EventCast(Event* event)
			{
				if (event && event->GetEventType() == T::GetStaticType())
				{
					return static_cast<T*>(event);
				}
				return nullptr;
			}

			template <typename T>
			const T* EventCast(const Event* event)
			{
				if (event && event->GetEventType() == T::GetStaticType())
				{
					return static_cast<const T*>(event);
				}
				return nullptr;
			}
		}
	}
}

#endif // DIA_EVENT_H
