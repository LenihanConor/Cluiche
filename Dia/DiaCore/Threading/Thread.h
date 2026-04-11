#ifndef DIA_THREAD_H
#define DIA_THREAD_H

#include "DiaCore/Core/Assert.h"
#include <thread>
#include <functional>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Thread
		//
		// Cross-platform thread wrapper around std::thread with additional utilities.
		//
		// USAGE:
		//   Thread thread("MyThread", []() {
		//       // Thread work here
		//   });
		//   thread.Join();
		//
		// FEATURES:
		//   - Named threads for debugging
		//   - Thread ID tracking
		//   - Joinable/detachable
		//   - Priority control (platform-dependent)
		//---------------------------------------------------------------------------------------------------------------------------------

		enum class ThreadPriority
		{
			Low,
			Normal,
			High,
			Critical
		};

		class Thread
		{
		public:
			using ThreadFunc = std::function<void()>;

			// Constructors
			Thread()
				: mName("Unnamed")
				, mJoinable(false)
			{}

			// Create thread with name and function
			Thread(const char* name, ThreadFunc func)
				: mName(name)
				, mJoinable(false)
			{
				Start(func);
			}

			// Destructor - joins if joinable
			~Thread()
			{
				if (mThread.joinable())
				{
					DIA_ASSERT(false, "Thread destroyed without join/detach");
					mThread.detach();
				}
			}

			// Start the thread
			void Start(ThreadFunc func)
			{
				DIA_ASSERT(!mThread.joinable(), "Thread already started");

				mJoinable = true;
				mThread = std::thread([this, func]() {
					// Set thread name for debugging (platform-specific, could be implemented)
					// SetPlatformThreadName(mName);

					// Run the actual function
					func();
				});
			}

			// Wait for thread to complete
			void Join()
			{
				if (mThread.joinable())
				{
					mThread.join();
					mJoinable = false;
				}
			}

			// Detach thread (runs independently)
			void Detach()
			{
				if (mThread.joinable())
				{
					mThread.detach();
					mJoinable = false;
				}
			}

			// Check if thread is joinable
			bool IsJoinable() const
			{
				return mThread.joinable();
			}

			// Get thread ID
			std::thread::id GetId() const
			{
				return mThread.get_id();
			}

			// Get thread name
			const char* GetName() const
			{
				return mName;
			}

			// Set thread priority (platform-specific implementation needed)
			void SetPriority(ThreadPriority priority)
			{
				// TODO: Platform-specific implementation
				// Windows: SetThreadPriority()
				// Linux: pthread_setschedparam()
				(void)priority; // Unused for now
			}

			// Get native handle
			std::thread::native_handle_type GetNativeHandle()
			{
				return mThread.native_handle();
			}

			// Get hardware concurrency (number of CPU cores)
			static unsigned int GetHardwareConcurrency()
			{
				return std::thread::hardware_concurrency();
			}

		private:
			// Prevent copying
			Thread(const Thread&) = delete;
			Thread& operator=(const Thread&) = delete;

			std::thread mThread;
			const char* mName;
			bool mJoinable;
		};

		//-----------------------------------------------------------------------------
		// This Thread utilities
		//-----------------------------------------------------------------------------
		namespace ThisThread
		{
			// Get current thread ID
			inline std::thread::id GetId()
			{
				return std::this_thread::get_id();
			}

			// Yield execution to other threads
			inline void Yield()
			{
				std::this_thread::yield();
			}

			// Sleep for specified milliseconds
			inline void SleepMs(unsigned int milliseconds)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
			}

			// Sleep for specified microseconds
			inline void SleepUs(unsigned int microseconds)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
			}

			// Sleep until specified time point
			template <typename Clock, typename Duration>
			inline void SleepUntil(const std::chrono::time_point<Clock, Duration>& time)
			{
				std::this_thread::sleep_until(time);
			}
		}
	}
}

#endif // DIA_THREAD_H
