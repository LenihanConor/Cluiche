#include "DiaObservation/Log/Logger.h"
#include "DiaObservation/Log/ThreadLogBuffer.h"
#include "DiaObservation/Log/ISink.h"
#include "DiaObservation/Log/LogEntry.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Dia
{
	namespace Observation { namespace Log
	{
		static thread_local ThreadLogBuffer* tLocalBuffer = nullptr;
		static thread_local bool tFlushing = false;

		Logger& Logger::Instance()
		{
			static Logger sInstance;
			return sInstance;
		}

		Logger::Logger()
			: mThreadBufferCount(0)
			, mSinkCount(0)
			, mDrainRunning(false)
		{
			memset(mThreadBuffers, 0, sizeof(mThreadBuffers));
			memset(mSinks, 0, sizeof(mSinks));
		}

		Logger::~Logger()
		{
			Stop();
		}

		void Logger::RegisterSink(ISink* sink)
		{
			if (sink == nullptr)
				return;

			{
				std::lock_guard<std::mutex> lock(mRegistryMutex);
				if (mSinkCount >= kMaxSinks)
					return;
				for (unsigned int i = 0; i < mSinkCount; ++i)
				{
					if (mSinks[i] == sink)
						return;
				}
				mSinks[mSinkCount++] = sink;
			}

			// Start drain thread on first sink registration.
			std::call_once(mDrainOnce, [this]
			{
				mDrainRunning.store(true, std::memory_order_release);
				mDrainThread = std::thread(&Logger::DrainLoop, this);
#ifdef _WIN32
				SetThreadDescription(mDrainThread.native_handle(), L"DiaObservation::Log::Drain");
#endif
			});
		}

		void Logger::UnregisterSink(ISink* sink)
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);
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

		void Logger::InternalFlush()
		{
			if (tFlushing)
				return;

			tFlushing = true;
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			LogEntry entry;
			for (unsigned int b = 0; b < mThreadBufferCount; ++b)
			{
				ThreadLogBuffer* buffer = mThreadBuffers[b];
				if (buffer == nullptr)
					continue;

				unsigned int drainLimit = buffer->PendingCount();
				unsigned int drained = 0;
				while (drained < drainLimit && buffer->Pop(entry))
				{
					++drained;
					for (unsigned int s = 0; s < mSinkCount; ++s)
					{
						if (mSinks[s] != nullptr && mSinks[s]->AcceptsEntry(entry))
						{
							mSinks[s]->OnLogEntry(entry);
						}
					}
				}
			}

			tFlushing = false;
		}

		void Logger::FlushBuffers()
		{
			// No-op: drain thread owns flushing. Left for source-level backward compatibility.
		}

		void Logger::Stop()
		{
			if (!mDrainRunning.exchange(false, std::memory_order_acq_rel))
				return;

			if (mDrainThread.joinable())
				mDrainThread.join();
		}

		void Logger::DrainLoop()
		{
			while (mDrainRunning.load(std::memory_order_acquire))
			{
				InternalFlush();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			InternalFlush(); // Final drain after stop signal
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
} // namespace Observation
} // namespace Dia
