#pragma once

#include <DiaObservation/Profile/TripRecord.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <thread>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Hands trip records from a ProcessingUnit thread to a dedicated writer thread that appends
		// them to <session dir>/trips.jsonl. The PU thread never performs file I/O: Submit() copies
		// the record into a bounded queue under a short mutex and returns. A full queue drops the new
		// record and counts it. Without a started session Submit() only counts.
		//
		// Drain mechanism: own writer thread, started/stopped by SessionManager (always on, unlike the
		// profiler drain thread which only runs when profiling is enabled).
		class TripReporter
		{
		public:
			static constexpr uint32_t kQueueCapacity = 8;

			TripReporter();
			~TripReporter();
			TripReporter(const TripReporter&) = delete;
			TripReporter& operator=(const TripReporter&) = delete;

			// sessionDir must end in a path separator (SessionManager::GetSessionDirectory()).
			// Returns false (and counts a write failure) if trips.jsonl cannot be opened.
			bool Start(const char* sessionDir);
			void Stop();   // flushes everything queued, joins the writer
			bool IsStarted() const { return mActive.load(std::memory_order_acquire); }

			// Non-blocking. Returns true when queued for writing.
			bool Submit(const TripRecord& record);

			// Submit + one Warning log line (call on the submitting side; the detector rate-limits trips).
			bool Report(const TripRecord& record);

			uint64_t GetSubmitted() const     { return mSubmitted.load(std::memory_order_relaxed); }
			uint64_t GetDropped() const       { return mDropped.load(std::memory_order_relaxed); }
			uint64_t GetWritten() const       { return mWritten.load(std::memory_order_relaxed); }
			uint64_t GetWriteFailures() const { return mWriteFailures.load(std::memory_order_relaxed); }

			void ResetCounters();   // test aid

			// Test aid: holds the writer so tests can fill the queue deterministically.
			void SetWriterPausedForTest(bool paused);

		private:
			void WriterLoop();
			void WriteOne(const TripRecord& record);
			void NoteWriteFailure(const char* what);
			void PublishMetrics();

			std::mutex              mMutex;
			std::condition_variable mWake;
			TripRecord*             mQueue;       // kQueueCapacity slots, heap (records are large)
			uint32_t                mHead;
			uint32_t                mCount;
			bool                    mStopRequested;
			bool                    mPaused;

			std::thread             mThread;
			std::atomic<bool>       mActive;
			std::FILE*              mFile;        // writer thread only (opened in Start, closed after join)

			std::atomic<uint64_t>   mSubmitted;
			std::atomic<uint64_t>   mDropped;
			std::atomic<uint64_t>   mWritten;
			std::atomic<uint64_t>   mWriteFailures;
		};
	}}
}
