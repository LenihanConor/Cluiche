#ifdef DIA_DEBUG
#include <DiaVisualDebugger/Console/DomainStateSnapshotPublisher.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <chrono>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			namespace
			{
				uint64_t DefaultNowMs()
				{
					return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now().time_since_epoch()).count());
				}
			}

			DomainStateSnapshotPublisher::DomainStateSnapshotPublisher(const DiaDebugDomainRegistry& registry, IDomainStateSink& sink,
				const ISchemaKeyResolver* resolver, NowMsFn now, uint32_t maxBytes)
				: mRegistry(registry), mSink(sink), mResolver(resolver), mNow(now ? now : &DefaultNowMs), mMaxBytes(maxBytes),
				  mScratch(new DomainStateSnapshot())
			{
				auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
				mMetricPublished = metrics.RegisterCounter(Dia::Core::StringCRC("dia.debugvisualiser.domain_snapshot.published"));
				mMetricBytes = metrics.RegisterGauge(Dia::Core::StringCRC("dia.debugvisualiser.domain_snapshot.bytes"));
				mMetricTruncated = metrics.RegisterCounter(Dia::Core::StringCRC("dia.debugvisualiser.domain_snapshot.truncated"));
			}

			void DomainStateSnapshotPublisher::Arm(uint32_t windowMs)
			{
				const uint64_t until = mNow() + windowMs;
				if (!mArmed)
				{
					DIA_LOG_INFO("DebugVis", "DomainStateSnapshot: interest started; publishing for %u ms", windowMs);
					mArmed = true;
					mArmedUntilMs = until;
				}
				else if (until > mArmedUntilMs)
				{
					mArmedUntilMs = until;
				}
			}

			void DomainStateSnapshotPublisher::Update(uint64_t simFrame)
			{
				if (!mArmed) { return; }
				const uint64_t now = mNow();
				if (now > mArmedUntilMs)
				{
					mArmed = false;
					DIA_LOG_INFO("DebugVis", "DomainStateSnapshot: interest stopped (%u snapshots published)", mPublishCount);
					return;
				}

				DIA_PROFILE_SCOPE("debugvisualiser.domain_snapshot.publish", ::Dia::Observation::Profile::Category::kDiaGraphics);
				const PackResult result = PackDomainStates(mRegistry, mResolver, simFrame, *mScratch, mMaxBytes);
				if (result.truncated)
				{
					++mTruncatedCount;
					if (mMetricTruncated) { mMetricTruncated->Inc(1); }
					if (!mHaveWarned || now - mLastWarnMs >= kTruncatedWarnIntervalMs)
					{
						mHaveWarned = true;
						mLastWarnMs = now;
						++mWarningCount;
						DIA_LOG_WARNING("DebugVis", "DomainStateSnapshot: truncated, %u domain(s) dropped to fit %u bytes (%u included)",
							result.droppedCount, mMaxBytes, result.includedCount);
					}
				}
				mSink.Write(*mScratch);
				++mPublishCount;
				if (mMetricPublished) { mMetricPublished->Inc(1); }
				if (mMetricBytes) { mMetricBytes->Set(static_cast<double>(mScratch->jsonBytes)); }
			}

			DomainStateHealth::DomainStateHealth()
			{
				auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
				mMetricPending = metrics.RegisterCounter(Dia::Core::StringCRC("dia.debugvisualiser.domain_snapshot.reads_pending"));
				mMetricAge = metrics.RegisterGauge(Dia::Core::StringCRC("dia.debugvisualiser.domain_snapshot.age_ms"));
			}

			void DomainStateHealth::OnPending()
			{
				mConsecutivePending.fetch_add(1, std::memory_order_relaxed);
				if (mMetricPending) { mMetricPending->Inc(1); }
			}

			void DomainStateHealth::OnSnapshot(uint64_t ageMs)
			{
				mConsecutivePending.store(0, std::memory_order_relaxed);
				mAgeMs.store(ageMs, std::memory_order_relaxed);
				if (mMetricAge) { mMetricAge->Set(static_cast<double>(ageMs)); }
			}

			Dia::Core::StringCRC DomainStateHealth::GetReporterName() const
			{
				return Dia::Core::StringCRC("dia.debugvisualiser.domain_snapshot");
			}

			Dia::Observation::Health::Health DomainStateHealth::Report() const
			{
				Dia::Observation::Health::Health h{};
				h.status = Dia::Observation::Health::HealthStatus::kOK;
				h.reason = Dia::Core::StringCRC::kZero;
				if (mConsecutivePending.load(std::memory_order_relaxed) >= kPendingLimit)
				{
					h.status = Dia::Observation::Health::HealthStatus::kDegraded;
					h.reason = Dia::Core::StringCRC("domain_snapshot_never_published");
				}
				else if (mAgeMs.load(std::memory_order_relaxed) > kStaleMs)
				{
					h.status = Dia::Observation::Health::HealthStatus::kDegraded;
					h.reason = Dia::Core::StringCRC("domain_snapshot_stale");
				}
				return h;
			}
		}
	}
}

#endif // DIA_DEBUG
