#pragma once

#include <DiaLogger/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#include <mutex>

namespace Dia
{
	namespace Logger
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

			void FlushBuffers();

			void Log(LogLevel level, const Dia::Core::StringCRC& channel,
				const char* fmt, ...);

		private:
			Logger();
			~Logger();

			Logger(const Logger&) = delete;
			Logger& operator=(const Logger&) = delete;

			static const unsigned int kMaxThreadBuffers = 8;
			static const unsigned int kMaxSinks = 8;

			ThreadLogBuffer* mThreadBuffers[kMaxThreadBuffers];
			unsigned int mThreadBufferCount;

			ISink* mSinks[kMaxSinks];
			unsigned int mSinkCount;

			std::mutex mRegistryMutex;
		};
	}
}
