#pragma once

// Test helper (Testing/ convention): deterministic tick + label source so tests do not hand-write labels.
// Each Next() is one weekly sample. Label "Y<year> W<week>" (both 1-based, 52 weeks per year).
// Band: 0 = Week (every sample), 1 = Year (the first week of each year, i.e. every 52nd sample).
// FakeClockSample mirrors the planned SampleInfo { uint64 tick; const char* label; uint8 band; }.
// Label pointers stay valid for the life of the clock.

#include <deque>
#include <stdint.h>
#include <stdio.h>
#include <string>

namespace Dia::CellHistory::Testing
{
	struct FakeClockSample
	{
		uint64_t		tick = 0;
		const char*		label = nullptr;
		uint8_t			band = 0;
	};

	class FakeClock
	{
	public:
		static const uint8_t	kBandWeek = 0;
		static const uint8_t	kBandYear = 1;
		static const uint32_t	kWeeksPerYear = 52;

		// The first Next() returns week index startWeek (0 = Y1 W1, a Year-band sample).
		explicit FakeClock(uint64_t ticksPerWeek = 7, uint32_t startWeek = 0, uint64_t startTick = 0)
			: mTicksPerWeek(ticksPerWeek), mWeek(startWeek), mStartWeek(startWeek), mStartTick(startTick) {}

		FakeClockSample Next()
		{
			const uint32_t w = mWeek++;
			char buf[32];
			snprintf(buf, sizeof(buf), "Y%u W%u", w / kWeeksPerYear + 1, w % kWeeksPerYear + 1);
			mLabels.emplace_back(buf);
			FakeClockSample s;
			s.tick = mStartTick + uint64_t(w - mStartWeek) * mTicksPerWeek;
			s.label = mLabels.back().c_str();
			s.band = (w % kWeeksPerYear == 0) ? kBandYear : kBandWeek;
			return s;
		}

		uint32_t SamplesIssued() const { return mWeek - mStartWeek; }

	private:
		uint64_t				mTicksPerWeek;
		uint32_t				mWeek;
		uint32_t				mStartWeek;
		uint64_t				mStartTick;
		std::deque<std::string>	mLabels;
	};
}
