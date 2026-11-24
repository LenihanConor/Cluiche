#pragma once

#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaCore/CRC/StringCRC.h>

#include <mutex>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Health
	{
		class IHealthTransitionSink;

		class HealthRegistry
		{
		public:
			static const unsigned int kMaxReporters = 64;

			static HealthRegistry& Instance();

			void Register(IHealthReporter* reporter);
			void Unregister(IHealthReporter* reporter);

			void RegisterTransitionSink(IHealthTransitionSink* sink);
			void UnregisterTransitionSink(IHealthTransitionSink* sink);

			struct Transition
			{
				Dia::Core::StringCRC reporterName;
				HealthStatus         oldStatus;
				HealthStatus         newStatus;
				Dia::Core::StringCRC reason;
			};

			struct ReporterSnapshot
			{
				Dia::Core::StringCRC name;
				Health               health;
			};

			void Snapshot(ReporterSnapshot* out, unsigned int maxCount, unsigned int& outCount) const;
			void PollTransitions(Transition* out, unsigned int maxCount, unsigned int& outCount);

			// Number of Register() calls rejected because the registry was full (cleared by Reset).
			unsigned int GetDroppedCount() const;

			void Reset();

		private:
			HealthRegistry();
			~HealthRegistry() = default;

			HealthRegistry(const HealthRegistry&) = delete;
			HealthRegistry& operator=(const HealthRegistry&) = delete;

			struct ReporterEntry
			{
				IHealthReporter* reporter;
				HealthStatus     lastKnownStatus;
			};

			struct RetiredEntry
			{
				Dia::Core::StringCRC name;
				Health               health;
			};

			ReporterEntry    mReporters[kMaxReporters];
			unsigned int     mReporterCount;
			unsigned int     mDroppedCount;

			RetiredEntry     mRetired[kMaxReporters];
			unsigned int     mRetiredCount;

			static const unsigned int kMaxTransitionSinks = 4;
			IHealthTransitionSink* mTransitionSinks[kMaxTransitionSinks];
			unsigned int           mTransitionSinkCount;

			mutable std::mutex mMutex;
		};
	}
} // namespace Observation
} // namespace Dia
