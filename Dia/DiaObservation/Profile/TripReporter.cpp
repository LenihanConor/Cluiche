#include "DiaObservation/Profile/TripReporter.h"
#include "DiaObservation/Log/DiaLog.h"
#include "DiaObservation/Metric/MetricRegistry.h"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Dia
{
	namespace Observation { namespace Profile
	{
		TripReporter::TripReporter()
			: mQueue(new TripRecord[kQueueCapacity])
			, mHead(0)
			, mCount(0)
			, mStopRequested(false)
			, mPaused(false)
			, mActive(false)
			, mFile(nullptr)
			, mSubmitted(0)
			, mDropped(0)
			, mWritten(0)
			, mWriteFailures(0)
		{
		}

		TripReporter::~TripReporter()
		{
			Stop();
			delete[] mQueue;
		}

		void TripReporter::ResetCounters()
		{
			mSubmitted.store(0);
			mDropped.store(0);
			mWritten.store(0);
			mWriteFailures.store(0);
		}

		void TripReporter::SetWriterPausedForTest(bool paused)
		{
			{
				std::lock_guard<std::mutex> lock(mMutex);
				mPaused = paused;
			}
			mWake.notify_all();
		}

		bool TripReporter::Start(const char* sessionDir)
		{
			if (mActive.load(std::memory_order_acquire) || !sessionDir || sessionDir[0] == '\0')
				return false;

			char path[640];
			snprintf(path, sizeof(path), "%strips.jsonl", sessionDir);
			std::FILE* f = nullptr;
			fopen_s(&f, path, "ab");
			if (!f)
			{
				NoteWriteFailure("open");
				return false;
			}

			{
				std::lock_guard<std::mutex> lock(mMutex);
				mHead = 0;
				mCount = 0;
				mStopRequested = false;
			}
			mFile = f;
			PublishMetrics();   // registers perf.trips.* so they exist from session start
			mActive.store(true, std::memory_order_release);
			mThread = std::thread(&TripReporter::WriterLoop, this);
#ifdef _WIN32
			SetThreadDescription(mThread.native_handle(), L"DiaObservation::Trip::Writer");
#endif
			return true;
		}

		void TripReporter::Stop()
		{
			if (!mActive.exchange(false, std::memory_order_acq_rel))
				return;

			{
				std::lock_guard<std::mutex> lock(mMutex);
				mStopRequested = true;
			}
			mWake.notify_all();
			if (mThread.joinable())
				mThread.join();

			if (mFile)
			{
				fclose(mFile);
				mFile = nullptr;
			}
		}

		bool TripReporter::Submit(const TripRecord& record)
		{
			mSubmitted.fetch_add(1, std::memory_order_relaxed);
			if (!mActive.load(std::memory_order_acquire))
				return false;

			bool queued = false;
			{
				std::lock_guard<std::mutex> lock(mMutex);
				if (mCount < kQueueCapacity)
				{
					mQueue[(mHead + mCount) % kQueueCapacity] = record;
					++mCount;
					queued = true;
				}
			}
			if (queued)
			{
				mWake.notify_one();
			}
			else
			{
				mDropped.fetch_add(1, std::memory_order_relaxed);
				PublishMetrics();
			}
			return queued;
		}

		bool TripReporter::Report(const TripRecord& r)
		{
			const bool queued = Submit(r);
			const char* topName = r.topCount > 0 ? r.top[0].name : "none";
			const float topMs = r.topCount > 0 ? r.top[0].ms : 0.0f;
			DIA_LOG_WARNING("perf", "pu.trip id=%s frames=%u-%u peak_ms=%.2f top=%s:%.2f",
				r.unit, r.firstFrame, r.lastFrame, r.peakMs, topName, topMs);
			return queued;
		}

		void TripReporter::WriterLoop()
		{
			TripRecord* local = new TripRecord;
			for (;;)
			{
				bool have = false;
				bool stop = false;
				{
					std::unique_lock<std::mutex> lock(mMutex);
					mWake.wait(lock, [this] { return mStopRequested || (mCount > 0 && !mPaused); });
					stop = mStopRequested;
					if (mCount > 0)
					{
						*local = mQueue[mHead];
						mHead = (mHead + 1) % kQueueCapacity;
						--mCount;
						have = true;
					}
				}
				if (have)
					WriteOne(*local);
				else if (stop)
					break;   // stop requested and queue empty
			}
			delete local;
		}

		void TripReporter::WriteOne(const TripRecord& record)
		{
			char line[16384];
			size_t len = 0;
			SerializeTripRecord(record, line, sizeof(line), len);
			if (!mFile || len == 0 || fwrite(line, 1, len, mFile) != len || fflush(mFile) != 0)
			{
				NoteWriteFailure("write");
				return;
			}
			mWritten.fetch_add(1, std::memory_order_relaxed);
		}

		void TripReporter::NoteWriteFailure(const char* what)
		{
			const uint64_t n = mWriteFailures.fetch_add(1, std::memory_order_relaxed) + 1;
			PublishMetrics();
			// Rate limited: first failure, then every 64th.
			if (n == 1 || (n % 64) == 0)
				DIA_LOG_WARNING("perf", "trips.jsonl %s failed (total failures: %llu)", what,
					static_cast<unsigned long long>(n));
		}

		void TripReporter::PublishMetrics()
		{
			// Looked up each time: registry Reset() invalidates cached pointers. Only runs on drop/failure.
			Metric::MetricRegistry& reg = Metric::MetricRegistry::Instance();
			if (Metric::Gauge* g = reg.RegisterGauge(Dia::Core::StringCRC("perf.trips.dropped")))
				g->Set(static_cast<double>(mDropped.load(std::memory_order_relaxed)));
			if (Metric::Gauge* g = reg.RegisterGauge(Dia::Core::StringCRC("perf.trips.write_failures")))
				g->Set(static_cast<double>(mWriteFailures.load(std::memory_order_relaxed)));
		}
	}}
}
