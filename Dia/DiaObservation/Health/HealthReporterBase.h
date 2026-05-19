#pragma once

#include <DiaObservation/Health/IHealthReporter.h>
#include <atomic>
#include <mutex>

namespace Dia
{
	namespace Observation { namespace Health
	{
		class HealthReporterBase : public IHealthReporter
		{
		public:
			HealthReporterBase();
			virtual ~HealthReporterBase() = default;

			Health Report() const override;

			void SetFailing(const Dia::Core::StringCRC& reason);
			void SetDegraded(const Dia::Core::StringCRC& reason);
			void SetOK();

			void IncrementErrors();
			void IncrementWarnings();

		protected:
			std::atomic<uint8_t>  mStatus;
			std::atomic<uint32_t> mErrors;
			std::atomic<uint32_t> mWarnings;

		private:
			mutable std::mutex   mReasonMutex;
			Dia::Core::StringCRC mReason;
		};
	}
} // namespace Observation
} // namespace Dia
