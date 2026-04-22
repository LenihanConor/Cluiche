#include "DiaLogger/Logger.h"
#include "DiaLogger/ThreadLogBuffer.h"
#include "DiaLogger/ISink.h"
#include "DiaLogger/LogEntry.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <mutex>

namespace Dia
{
	namespace Logger
	{
		static thread_local ThreadLogBuffer* tLocalBuffer = nullptr;

		Logger& Logger::Instance()
		{
			static Logger sInstance;
			return sInstance;
		}

		Logger::Logger()
			: mThreadBufferCount(0)
			, mSinkCount(0)
		{
			memset(mThreadBuffers, 0, sizeof(mThreadBuffers));
			memset(mSinks, 0, sizeof(mSinks));
		}

		Logger::~Logger()
		{
		}

		void Logger::RegisterSink(ISink* sink)
		{
			if (sink == nullptr || mSinkCount >= kMaxSinks)
				return;

			for (unsigned int i = 0; i < mSinkCount; ++i)
			{
				if (mSinks[i] == sink)
					return;
			}

			mSinks[mSinkCount++] = sink;
		}

		void Logger::UnregisterSink(ISink* sink)
		{
			for (unsigned int i = 0; i < mSinkCount; ++i)
			{
				if (mSinks[i] == sink)
				{
					mSinks[i] = mSinks[mSinkCount - 1];
					mSinks[mSinkCount - 1] = nullptr;
					--mSinkCount;
					return;
				}
			}
		}

		void Logger::RegisterThreadBuffer()
		{
			if (tLocalBuffer != nullptr)
				return;

			tLocalBuffer = new ThreadLogBuffer();

			std::lock_guard<std::mutex> lock(mRegistryMutex);
			if (mThreadBufferCount < kMaxThreadBuffers)
			{
				mThreadBuffers[mThreadBufferCount++] = tLocalBuffer;
			}
		}

		void Logger::UnregisterThreadBuffer()
		{
			if (tLocalBuffer == nullptr)
				return;

			{
				std::lock_guard<std::mutex> lock(mRegistryMutex);
				for (unsigned int i = 0; i < mThreadBufferCount; ++i)
				{
					if (mThreadBuffers[i] == tLocalBuffer)
					{
						mThreadBuffers[i] = mThreadBuffers[mThreadBufferCount - 1];
						mThreadBuffers[mThreadBufferCount - 1] = nullptr;
						--mThreadBufferCount;
						break;
					}
				}
			}

			delete tLocalBuffer;
			tLocalBuffer = nullptr;
		}

		void Logger::FlushBuffers()
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			LogEntry entry;
			for (unsigned int b = 0; b < mThreadBufferCount; ++b)
			{
				ThreadLogBuffer* buffer = mThreadBuffers[b];
				if (buffer == nullptr)
					continue;

				while (buffer->Pop(entry))
				{
					for (unsigned int s = 0; s < mSinkCount; ++s)
					{
						if (mSinks[s] != nullptr && mSinks[s]->AcceptsEntry(entry))
						{
							mSinks[s]->OnLogEntry(entry);
						}
					}
				}
			}
		}

		void Logger::Log(LogLevel level, const Dia::Core::StringCRC& channel,
			const char* fmt, ...)
		{
			if (tLocalBuffer == nullptr)
				return;

			LogEntry entry;
			entry.level = level;
			entry.channel = channel;

			va_list args;
			va_start(args, fmt);
			vsnprintf(entry.message, sizeof(entry.message), fmt, args);
			va_end(args);

			tLocalBuffer->Push(entry);
		}

		void Logger::DispatchImmediate(const LogEntry& entry)
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			for (unsigned int s = 0; s < mSinkCount; ++s)
			{
				if (mSinks[s] != nullptr && mSinks[s]->AcceptsEntry(entry))
				{
					mSinks[s]->OnLogEntry(entry);
				}
			}
		}
	}
}
