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

		typedef void (*RetentionCallback)(const LogEntry& entry, void* userData);

		class Logger
		{
		public:
			static Logger& Instance();

			void RegisterSink(ISink* sink);
			void UnregisterSink(ISink* sink);

			void RegisterThreadBuffer();
			void UnregisterThreadBuffer();

			void SetRetentionCallback(RetentionCallback callback, void* userData);

			// Configure global minimum log level (entries below this are dropped).
			void SetMinLevel(LogLevel level);
			LogLevel GetMinLevel() const;

			// Configure per-channel log level override (max 16 channels).
			void SetChannelOverride(const Dia::Core::StringCRC& channel, LogLevel level);

			// No-op for external callers — drain thread handles flushing.
			// Left in the public API for source-level backward compatibility.
			void FlushBuffers();

			// Synchronous flush — blocks until all pending entries are dispatched.
			// Use in tests only; production code relies on the async drain.
			void FlushSync();

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

			bool PassesFilter(LogLevel level, const Dia::Core::StringCRC& channel) const;
			void InternalFlush();
			void DrainLoop();

			static const unsigned int kMaxThreadBuffers = 8;
			static const unsigned int kMaxSinks = 8;
			static const unsigned int kMaxChannelOverrides = 16;

			ThreadLogBuffer* mThreadBuffers[kMaxThreadBuffers];
			unsigned int mThreadBufferCount;

			ISink* mSinks[kMaxSinks];
			unsigned int mSinkCount;

			RetentionCallback mRetentionCallback;
			void* mRetentionUserData;

			LogLevel mMinLevel;

			struct ChannelLevelOverride
			{
				Dia::Core::StringCRC channel;
				LogLevel             level;
			};
			ChannelLevelOverride mChannelOverrides[kMaxChannelOverrides];
			unsigned int mChannelOverrideCount;

			std::mutex mRegistryMutex;

			std::once_flag     mDrainOnce;
			std::thread        mDrainThread;
			std::atomic<bool>  mDrainRunning;
		};
	}
} // namespace Observation
} // namespace Dia
