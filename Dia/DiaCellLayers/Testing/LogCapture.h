#pragma once

// Test helper (Testing/ convention, no GoogleTest): captures DiaObservation log entries while alive. Scope it around the
// code under test, then query counts; every query flushes the logger first so async dispatch cannot hide an entry.

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>

#include <mutex>
#include <string>
#include <vector>

namespace Dia::CellLayers::Testing
{
	class LogCapture : public Dia::Observation::Log::ISink
	{
	public:
		typedef Dia::Observation::Log::LogLevel Level;

		explicit LogCapture(const char* channel = "CEL")
		{
			SetLevelThreshold(Level::kTrace);
			SetChannelFilter(Dia::Core::StringCRC(channel), true);
			Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
			Dia::Observation::Log::Logger::Instance().RegisterSink(this);
		}
		~LogCapture() override
		{
			Dia::Observation::Log::Logger::Instance().FlushSync();
			Dia::Observation::Log::Logger::Instance().UnregisterSink(this);
			Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
		}

		void OnLogEntry(const Dia::Observation::Log::LogEntry& e) override
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mEntries.push_back({ e.level, e.message });
		}
		const char* GetName() const override { return "CellLayersLogCapture"; }

		void Clear()
		{
			Dia::Observation::Log::Logger::Instance().FlushSync();
			std::lock_guard<std::mutex> lock(mMutex);
			mEntries.clear();
		}

		// Number of captured entries at exactly `level` whose message contains `substr` (empty = any).
		unsigned Count(Level level, const char* substr = "") const
		{
			Dia::Observation::Log::Logger::Instance().FlushSync();
			std::lock_guard<std::mutex> lock(mMutex);
			unsigned n = 0;
			for (const Entry& e : mEntries)
			{
				if (e.level == level && e.message.find(substr) != std::string::npos) { ++n; }
			}
			return n;
		}
		unsigned Warnings(const char* substr = "") const { return Count(Level::kWarning, substr); }
		unsigned Errors(const char* substr = "") const { return Count(Level::kError, substr); }

	private:
		struct Entry { Level level; std::string message; };
		mutable std::mutex	mMutex;
		std::vector<Entry>	mEntries;
	};
}
