#pragma once

#include <DiaObservation/Profile/ProfileCategory.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia
{
	namespace Observation
	{
		namespace Metric { class Histogram; }

		namespace Profile
		{
			class ScopedZone
			{
			public:
				ScopedZone(const Dia::Core::StringCRC& name, ProfileCategory category,
				           Dia::Observation::Metric::Histogram* histogram = nullptr);
				// Explicit subject (CRC value; 0 = inherit from the enclosing open scope).
				ScopedZone(const Dia::Core::StringCRC& name, ProfileCategory category, uint32_t subject,
				           Dia::Observation::Metric::Histogram* histogram = nullptr);
				~ScopedZone();

				ScopedZone(const ScopedZone&) = delete;
				ScopedZone& operator=(const ScopedZone&) = delete;

			private:
				uint64_t                             mScopeId;
				uint64_t                             mParentScopeId;
				uint64_t                             mStartNs;
				uint32_t                             mFrameNumber;
				Dia::Core::StringCRC                 mName;
				ProfileCategory                      mCategory;
				Dia::Observation::Metric::Histogram* mHistogram;
				bool                                 mActive;

				void Open(const Dia::Core::StringCRC& name, ProfileCategory category, uint32_t subject);
			};
		}
	} // namespace Observation
} // namespace Dia
