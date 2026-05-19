#include "DiaObservation/Health/HealthRegistry.h"
#include "DiaObservation/Health/IHealthTransitionSink.h"

#include <cstring>

namespace Dia
{
	namespace Observation { namespace Health
	{
		HealthRegistry& HealthRegistry::Instance()
		{
			static HealthRegistry sInstance;
			return sInstance;
		}

		HealthRegistry::HealthRegistry()
			: mReporterCount(0)
			, mRetiredCount(0)
			, mTransitionSinkCount(0)
		{
			std::memset(mReporters, 0, sizeof(mReporters));
			std::memset(mRetired, 0, sizeof(mRetired));
			std::memset(mTransitionSinks, 0, sizeof(mTransitionSinks));
		}

		void HealthRegistry::Register(IHealthReporter* reporter)
		{
			if (reporter == nullptr)
				return;

			std::lock_guard<std::mutex> lock(mMutex);

			for (unsigned int i = 0; i < mReporterCount; ++i)
			{
				if (mReporters[i].reporter == reporter)
					return;
			}

			if (mReporterCount >= kMaxReporters)
				return;

			mReporters[mReporterCount].reporter = reporter;
			mReporters[mReporterCount].lastKnownStatus = HealthStatus::kOK;
			++mReporterCount;
		}

		void HealthRegistry::Unregister(IHealthReporter* reporter)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			for (unsigned int i = 0; i < mReporterCount; ++i)
			{
				if (mReporters[i].reporter == reporter)
				{
					// Retain final snapshot so health.json includes reporters that
					// stopped before SessionManager::WriteHealthJson() runs.
					if (mRetiredCount < kMaxReporters)
					{
						mRetired[mRetiredCount].name   = reporter->GetReporterName();
						mRetired[mRetiredCount].health = reporter->Report();
						++mRetiredCount;
					}

					mReporters[i] = mReporters[mReporterCount - 1];
					std::memset(&mReporters[mReporterCount - 1], 0, sizeof(ReporterEntry));
					--mReporterCount;
					return;
				}
			}
		}

		void HealthRegistry::RegisterTransitionSink(IHealthTransitionSink* sink)
		{
			if (sink == nullptr)
				return;

			std::lock_guard<std::mutex> lock(mMutex);
			if (mTransitionSinkCount >= kMaxTransitionSinks)
				return;

			for (unsigned int i = 0; i < mTransitionSinkCount; ++i)
			{
				if (mTransitionSinks[i] == sink)
					return;
			}
			mTransitionSinks[mTransitionSinkCount++] = sink;
		}

		void HealthRegistry::UnregisterTransitionSink(IHealthTransitionSink* sink)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			for (unsigned int i = 0; i < mTransitionSinkCount; ++i)
			{
				if (mTransitionSinks[i] == sink)
				{
					mTransitionSinks[i] = mTransitionSinks[mTransitionSinkCount - 1];
					mTransitionSinks[mTransitionSinkCount - 1] = nullptr;
					--mTransitionSinkCount;
					return;
				}
			}
		}

		void HealthRegistry::Snapshot(ReporterSnapshot* out, unsigned int maxCount, unsigned int& outCount) const
		{
			std::lock_guard<std::mutex> lock(mMutex);
			outCount = 0;

			// Live reporters first
			for (unsigned int i = 0; i < mReporterCount && outCount < maxCount; ++i)
			{
				IHealthReporter* reporter = mReporters[i].reporter;
				if (reporter)
				{
					out[outCount].name   = reporter->GetReporterName();
					out[outCount].health = reporter->Report();
					++outCount;
				}
			}

			// Retired reporters (unregistered before this snapshot was taken)
			for (unsigned int i = 0; i < mRetiredCount && outCount < maxCount; ++i)
			{
				out[outCount].name   = mRetired[i].name;
				out[outCount].health = mRetired[i].health;
				++outCount;
			}
		}

		void HealthRegistry::PollTransitions(Transition* out, unsigned int maxCount, unsigned int& outCount)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			outCount = 0;

			for (unsigned int i = 0; i < mReporterCount && outCount < maxCount; ++i)
			{
				IHealthReporter* reporter = mReporters[i].reporter;
				if (!reporter)
					continue;

				Health current = reporter->Report();
				HealthStatus oldStatus = mReporters[i].lastKnownStatus;

				if (current.status != oldStatus)
				{
					out[outCount].reporterName = reporter->GetReporterName();
					out[outCount].oldStatus = oldStatus;
					out[outCount].newStatus = current.status;
					out[outCount].reason = current.reason;

					// Notify transition sinks
					for (unsigned int s = 0; s < mTransitionSinkCount; ++s)
					{
						if (mTransitionSinks[s])
							mTransitionSinks[s]->OnTransition(out[outCount]);
					}

					++outCount;
					mReporters[i].lastKnownStatus = current.status;
				}
			}
		}

		void HealthRegistry::Reset()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mReporterCount = 0;
			std::memset(mReporters, 0, sizeof(mReporters));
			mRetiredCount = 0;
			std::memset(mRetired, 0, sizeof(mRetired));
			mTransitionSinkCount = 0;
			std::memset(mTransitionSinks, 0, sizeof(mTransitionSinks));
		}
	}
} // namespace Observation
} // namespace Dia
