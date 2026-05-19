#pragma once

#include <DiaObservation/Profile/ScopeRecord.h>

#include <cstdio>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		class ProfileFileSink
		{
		public:
			ProfileFileSink(const char* jsonlPath, const char* finalPath,
			                const char* sessionId, int64_t epochOffsetNs);
			~ProfileFileSink();

			bool IsOpen() const { return mFile != nullptr; }
			void OnRecord(const ScopeRecord& record);
			void WriteFinalJson();   // called by Profiler::Stop(), writes last <=60 frames

		private:
			// In-memory frame ring: last 60 frames
			static const unsigned int kMaxFrames        = 60;
			static const unsigned int kMaxScopesPerFrame = 512;

			struct FrameData
			{
				uint32_t    frameNumber;
				ScopeRecord scopes[kMaxScopesPerFrame];
				unsigned int scopeCount;
			};

			FrameData    mFrameRing[kMaxFrames];
			unsigned int mFrameHead;        // points to oldest frame slot
			unsigned int mFrameCount;       // how many frames accumulated (<=60)
			int          mCurrentFrameSlot; // index into mFrameRing for current frame (-1 if none)
			uint32_t     mLastFrameNumber;

			FILE*   mFile;
			char    mFinalPath[512];
			char    mSessionId[32];
			int64_t mEpochOffsetNs;
		};
	}
} // namespace Observation
} // namespace Dia
