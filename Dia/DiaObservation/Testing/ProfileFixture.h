#pragma once

#include <DiaObservation/Profile/Profiler.h>

#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace Observation { namespace Testing
	{
		// Starts/stops the Profiler cleanly around each test.
		// Registers the calling thread's scope buffer so macros work inside tests.
		// Removes the temp profile file on destruction.
		struct ProfileFixture
		{
			char mJsonlPath[512];
			char mFinalPath[512];

			ProfileFixture()
			{
				std::memset(mJsonlPath, 0, sizeof(mJsonlPath));
				std::memset(mFinalPath, 0, sizeof(mFinalPath));

				strncpy_s(mJsonlPath, "profile-test.jsonl",  sizeof(mJsonlPath) - 1);
				strncpy_s(mFinalPath, "profile-test-final.json", sizeof(mFinalPath) - 1);

				// Ensure any previous run is stopped before starting fresh
				Profile::Profiler::Instance().Stop();

				Profile::Profiler::Instance().Start(
					mJsonlPath,
					Profile::Category::kAll,
					"test-session",
					0);

				Profile::Profiler::Instance().RegisterThreadScopeBuffer();
			}

			~ProfileFixture()
			{
				Profile::Profiler::Instance().UnregisterThreadScopeBuffer();
				Profile::Profiler::Instance().Stop();

				// Remove temp files
				if (mJsonlPath[0] != '\0')
					std::remove(mJsonlPath);
				if (mFinalPath[0] != '\0')
					std::remove(mFinalPath);
			}

			// Non-copyable
			ProfileFixture(const ProfileFixture&) = delete;
			ProfileFixture& operator=(const ProfileFixture&) = delete;
		};
	}
} // namespace Observation
} // namespace Dia
