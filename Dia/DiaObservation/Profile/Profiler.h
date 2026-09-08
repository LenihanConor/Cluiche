#pragma once

#include <DiaObservation/Profile/ScopeRecord.h>

#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		class ProfileFileSink;

		class Profiler
		{
		public:
			static Profiler& Instance();

			bool Start(const char* profileFilePath, ProfileCategory activeMask,
			           const char* sessionId, int64_t epochOffsetNs);
			void Stop();

			void BeginFrame();   // increments frame counter; signals drain of previous frame records
			void EndFrame();     // no-op in v1, reserved for GPU fence integration
			uint32_t GetCurrentFrame() const;

			void            SetActiveMask(ProfileCategory mask);
			ProfileCategory ActiveMask() const;

			void RegisterThreadScopeBuffer();
			void UnregisterThreadScopeBuffer();

			// Called by ScopedZone only
			uint64_t OnScopeOpen(const Dia::Core::StringCRC& name, ProfileCategory category,
			                     uint32_t& outFrameNumber, uint64_t& outParentScopeId);
			void     OnScopeClose(uint64_t scopeId, const Dia::Core::StringCRC& name,
			                      ProfileCategory category, uint64_t parentScopeId,
			                      uint64_t startNs, uint64_t durationNs,
			                      uint32_t frameNumber, uint32_t threadId);

			// Snapshot open scopes (for crash handler)
			struct OpenScopeEntry
			{
				Dia::Core::StringCRC name;
				uint64_t             startUnixNano;
				uint32_t             frameNumber;
				uint32_t             threadId;
			};
			unsigned int GetOpenScopesSnapshot(OpenScopeEntry* out, unsigned int maxOut) const;

			bool IsStarted() const { return mStarted.load(std::memory_order_acquire); }

		private:
			Profiler();
			~Profiler();
			Profiler(const Profiler&) = delete;
			Profiler& operator=(const Profiler&) = delete;

			void DrainLoop();
			void DrainAllRings(uint32_t beforeFrame);

			struct ThreadRingRef
			{
				ScopeRecord*               closedRing;
				std::atomic<unsigned int>* head;
				std::atomic<unsigned int>* tail;
				uint32_t*                  openCount;
				uint64_t*                  openScopeIds;    // parallel arrays of open stack
				uint64_t*                  openStartNs;
				Dia::Core::StringCRC*      openNames;
				uint32_t*                  openFrameNumbers;
				uint32_t                   threadId;
			};

			static const unsigned int kMaxThreadRings = 16;
			ThreadRingRef            mThreadRings[kMaxThreadRings];
			unsigned int             mThreadRingCount;
			mutable std::mutex       mRegistryMutex;

			ProfileFileSink*              mSink;
			std::atomic<ProfileCategory>  mActiveMask;
			std::atomic<uint32_t>         mCurrentFrame;

			std::thread       mDrainThread;
			std::atomic<bool> mDrainRunning;
			std::atomic<bool> mStarted;
			std::atomic<bool> mFrameAdvanced;
		};
	}
} // namespace Observation
} // namespace Dia
