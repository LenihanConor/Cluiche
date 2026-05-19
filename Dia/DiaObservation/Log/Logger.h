#pragma once

#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaCore/CRC/StringCRC.h>

#include <mutex>
#include <thread>
#include <atomic>

namespace Dia
{
	namespace Observation { namespace Log
	{
		class ISink;
		class ThreadLogBuffer;

		class Logger
		{
		public:
			static Logger& Instance();

			void RegisterSink(ISink* sink);
			void UnregisterSink(ISink* sink);

			void RegisterThreadBuffer();
			void UnregisterThreadBuffer();

			// No-op for external callers — drain thread handles flushing.
			// Left in the public API for source-level backward compatibility.
			void FlushBuffers();

			// Signal drain thread to stop, drain pending entries synchronously, join.
			// Must be called before unregistering sinks. Idempotent.
			void Stop();

			void Log(LogLevel level, const Dia::Core::StringCRC& channel,
				const char* fmt, ...);

			void DispatchImmediate(const LogEntry& entry);

		private:
			Logger();
			~Logger();

			Logger(const Logger&) = delete;
			Logger& operator=(const Logger&) = delete;

			void InternalFlush();
			void DrainLoop();

			static const unsigned int kMaxThreadBuffers = 8;
			static const unsigned int kMaxSinks = 8;

			ThreadLogBuffer* mThreadBuffers[kMaxThreadBuffers];
			unsigned int mThreadBufferCount;

			ISink* mSinks[kMaxSinks];
			unsigned int mSinkCount;

			std::mutex mRegistryMutex;

			std::once_flag     mDrainOnce;
			std::thread        mDrainThread;
			std::atomic<bool>  mDrainRunning;
		};
	}
} // namespace Observation
} // namespace Dia
