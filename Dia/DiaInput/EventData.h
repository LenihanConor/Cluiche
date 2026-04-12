////////////////////////////////////////////////////////////////////////////////
// Filename: EventData.h: A single frame of key strokes
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/Event.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Input
	{
		/// @brief Fixed-capacity event buffer for a single input frame
		///
		/// EventData is a bounded array that holds input events collected during
		/// one frame. The fixed capacity prevents unbounded allocations during polling.
		///
		/// **Capacity:** Configurable via template parameter (default: 64 events)
		///
		/// **Usage:**
		/// @code
		/// EventData events;  // Default: 64 events
		/// EventDataT<128> largeBuffer;  // Custom: 128 events
		///
		/// inputSourceManager.Update(events);  // Polls all sources
		///
		/// for (unsigned int i = 0; i < events.Size(); i++)
		/// {
		///     Event& event = events[i];
		///     switch (event.type)
		///     {
		///         case Event::EType::kKeyPressed:
		///             HandleKey(event.key.AsKey());
		///             break;
		///     }
		/// }
		/// @endcode
		///
		/// @note Default capacity (64) accommodates 4 gamepads × 14 buttons + mouse moves.
		///       If capacity is exceeded, InputSourceManager logs a warning.
		////////////////////////////////////////////////////////////////////////////////
		template <unsigned int Capacity = 64>
		class EventDataT : public Dia::Core::Containers::DynamicArrayC<Event, Capacity>
		{
		public:
			/// @brief Default constructor - creates empty event buffer
			EventDataT() = default;
		};

		/// @brief Default EventData with 64-event capacity
		///
		/// For backward compatibility, EventData uses 64 events (increased from 16).
		/// Use EventDataT<N> to customize capacity.
		using EventData = EventDataT<64>;
	}
}
