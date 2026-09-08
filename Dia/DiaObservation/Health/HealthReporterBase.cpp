#include "DiaObservation/Health/HealthReporterBase.h"

namespace Dia
{
	namespace Observation { namespace Health
	{
		HealthReporterBase::HealthReporterBase()
		{
			mStatus.store(static_cast<uint8_t>(HealthStatus::kOK), std::memory_order_relaxed);
			mErrors.store(0, std::memory_order_relaxed);
			mWarnings.store(0, std::memory_order_relaxed);
		}

		Health HealthReporterBase::Report() const
		{
			Health h;
			h.status   = static_cast<HealthStatus>(mStatus.load(std::memory_order_acquire));
			h.errors   = mErrors.load(std::memory_order_relaxed);
			h.warnings = mWarnings.load(std::memory_order_relaxed);
			{
				std::lock_guard<std::mutex> lk(mReasonMutex);
				h.reason = mReason;
			}
			return h;
		}

		void HealthReporterBase::SetFailing(const Dia::Core::StringCRC& reason)
		{
			{
				std::lock_guard<std::mutex> lk(mReasonMutex);
				mReason = reason;
			}
			mStatus.store(static_cast<uint8_t>(HealthStatus::kFailing), std::memory_order_release);
		}

		void HealthReporterBase::SetDegraded(const Dia::Core::StringCRC& reason)
		{
			{
				std::lock_guard<std::mutex> lk(mReasonMutex);
				mReason = reason;
			}
			mStatus.store(static_cast<uint8_t>(HealthStatus::kDegraded), std::memory_order_release);
		}

		void HealthReporterBase::SetOK()
		{
			{
				std::lock_guard<std::mutex> lk(mReasonMutex);
				mReason = Dia::Core::StringCRC();
			}
			mStatus.store(static_cast<uint8_t>(HealthStatus::kOK), std::memory_order_release);
		}

		void HealthReporterBase::IncrementErrors()
		{
			mErrors.fetch_add(1, std::memory_order_relaxed);
		}

		void HealthReporterBase::IncrementWarnings()
		{
			mWarnings.fetch_add(1, std::memory_order_relaxed);
		}
	}
} // namespace Observation
} // namespace Dia
