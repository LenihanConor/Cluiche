////////////////////////////////////////////////////////////////////////////////
// Filename: IInputSource.h: Interface to wrap the polling of input controller events
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaInput/EventData.h"

namespace Dia
{
	namespace Input
	{
		/// @brief Interface for input sources (keyboard, mouse, gamepad, etc.)
		///
		/// Implementations poll platform-specific APIs (SFML, XInput, etc.) and
		/// convert raw input into unified Event format.
		///
		/// **Lifecycle:** StartFrame() → Poll() → EndFrame()
		///
		/// **Threading:** Call from main thread only - not thread-safe.
		///
		/// **Priority:** Sources are polled in priority order (Highest → Lowest).
		///              UI input sources should use High/Highest priority to consume
		///              events before game input.
		///
		/// **Example:**
		/// @code
		/// class MyInputSource : public IInputSource
		/// {
		/// public:
		///     MyInputSource() : IInputSource(Priority::Normal) {}
		///
		///     void Poll(EventData& outStream) override
		///     {
		///         // Poll platform API and add events
		///         Event keyEvent;
		///         keyEvent.type = Event::EType::kKeyPressed;
		///         outStream.Add(keyEvent);
		///     }
		/// };
		/// @endcode
		////////////////////////////////////////////////////////////////////////////////
		class IInputSource
		{
		public:
			/// @brief Priority levels for input source polling order
			enum class Priority
			{
				Lowest  = 0,   ///< Polled last (e.g., background systems)
				Low     = 25,  ///< Lower priority (e.g., debug input)
				Normal  = 50,  ///< Default priority (e.g., gameplay input)
				High    = 75,  ///< Higher priority (e.g., UI overlay)
				Highest = 100  ///< Polled first (e.g., modal dialogs)
			};

			/// @brief Constructor with optional priority
			///
			/// @param priority Polling priority (default: Normal)
			IInputSource(Priority priority = Priority::Normal)
				: mPriority(priority)
			{
				static int sNextId = 0xFFFF;
				mId = sNextId++;
			}

			/// @brief Virtual destructor for proper cleanup
			virtual ~IInputSource(){}

			/// @brief Called at the beginning of each input polling frame
			///
			/// Optional hook for per-frame initialization (e.g., clearing buffers).
			virtual void StartFrame(){}

			/// @brief Poll input events and append to output stream
			///
			/// @param outStream Event buffer to populate with new events
			///
			/// @note Do not clear outStream - multiple sources append to same buffer
			virtual void Poll(EventData& outStream) = 0;

			/// @brief Called at the end of each input polling frame
			///
			/// Optional hook for per-frame cleanup (e.g., state refresh).
			virtual void EndFrame(){}

			/// @brief Get unique identifier for this input source
			///
			/// @return Unique integer ID assigned at construction
			int GetUniqueID()const { return mId; }

			/// @brief Get polling priority for this source
			///
			/// @return Priority value (higher = polled earlier)
			Priority GetPriority() const { return mPriority; }

			/// @brief Set polling priority for this source
			///
			/// @param priority New priority value
			void SetPriority(Priority priority) { mPriority = priority; }

		private:
			int mId;             ///< Unique identifier for this source
			Priority mPriority;  ///< Polling priority
		};
	}
}