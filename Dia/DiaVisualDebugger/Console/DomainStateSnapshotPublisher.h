////////////////////////////////////////////////////////////////////////////////
// Filename: DomainStateSnapshotPublisher.h
// Description: Producer side of the domain state snapshot (SimPU). Interest-gated: Main
//              signals interest (Arm) and the publisher packs every registered domain
//              (PackDomainStates) and writes one DomainStateSnapshot to a sink once per
//              Sim frame, only while the interest window is open. GetJSONState therefore
//              only ever runs on the thread that calls Update (the Sim thread).
//              Also holds the health reporter and Main-side metric hooks.
//              Pattern: Dia/DiaCellLayers/ViewerStateSnapshot.{h,cpp}.
//              No threading primitives are used between the producer and Main; the
//              only cross-thread state is the health reporter (atomics, as ViewerStateHealth).
// Feature spec: docs/specs/applications/dia/systems/diadebugserver/domain-state-snapshots.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/DomainStateSnapshot.h>
#include <DiaObservation/Health/IHealthReporter.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace Dia::Observation::Metric { class Counter; class Gauge; }

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			// Write side (SimPU); the host backs it with a FrameStream writer.
			class IDomainStateSink
			{
			public:
				virtual ~IDomainStateSink() = default;
				virtual void Write(const DomainStateSnapshot& snapshot) = 0;
			};

			// Wall-clock milliseconds (steady). Tests inject their own.
			typedef uint64_t (*NowMsFn)();

			class DomainStateSnapshotPublisher
			{
			public:
				static constexpr uint32_t kDefaultWindowMs = 2000;
				static constexpr uint64_t kTruncatedWarnIntervalMs = 1000;

				// registry, sink and resolver are borrowed (resolver may be null).
				DomainStateSnapshotPublisher(const DiaDebugDomainRegistry& registry, IDomainStateSink& sink,
											 const ISchemaKeyResolver* resolver = nullptr,
											 NowMsFn now = nullptr, uint32_t maxBytes = kMaxSnapshotBytes);
				DomainStateSnapshotPublisher(const DomainStateSnapshotPublisher&) = delete;
				DomainStateSnapshotPublisher& operator=(const DomainStateSnapshotPublisher&) = delete;

				// Interest seen now: keep publishing for windowMs after this call (a later Arm extends it).
				void Arm(uint32_t windowMs = kDefaultWindowMs);
				// Once per Sim frame. Packs and writes one snapshot while armed; does nothing otherwise.
				void Update(uint64_t simFrame);

				bool     IsArmed() const { return mArmed; }
				uint32_t PublishCount() const { return mPublishCount; }
				uint32_t TruncatedCount() const { return mTruncatedCount; }
				uint32_t WarningCount() const { return mWarningCount; }

			private:
				const DiaDebugDomainRegistry&	mRegistry;
				IDomainStateSink&				mSink;
				const ISchemaKeyResolver*		mResolver;
				NowMsFn							mNow;
				uint32_t						mMaxBytes;
				std::unique_ptr<DomainStateSnapshot> mScratch;	// ~128 KB: heap, never the stack
				bool							mArmed = false;
				uint64_t						mArmedUntilMs = 0;
				bool							mHaveWarned = false;
				uint64_t						mLastWarnMs = 0;
				uint32_t						mPublishCount = 0;
				uint32_t						mTruncatedCount = 0;
				uint32_t						mWarningCount = 0;
				Dia::Observation::Metric::Counter*	mMetricPublished = nullptr;	// dia.debugvisualiser.domain_snapshot.published
				Dia::Observation::Metric::Gauge*	mMetricBytes = nullptr;		// dia.debugvisualiser.domain_snapshot.bytes (last snapshot)
				Dia::Observation::Metric::Counter*	mMetricTruncated = nullptr;	// dia.debugvisualiser.domain_snapshot.truncated
			};

			// Health "dia.debugvisualiser.domain_snapshot" over Main's read outcomes (Main writes, any thread reads).
			//  - Degraded "domain_snapshot_never_published" after kPendingLimit consecutive reads that found no snapshot.
			//  - Degraded "domain_snapshot_stale" when the last read's age exceeds kStaleMs.
			// OnPending / OnSnapshot also drive the metrics reads_pending (counter) and age_ms (gauge).
			class DomainStateHealth : public Dia::Observation::Health::IHealthReporter
			{
			public:
				static constexpr uint32_t kPendingLimit = 30;
				static constexpr uint64_t kStaleMs = 5000;

				DomainStateHealth();

				Dia::Core::StringCRC				GetReporterName() const override;
				Dia::Observation::Health::Health	Report() const override;

				void OnPending();
				void OnSnapshot(uint64_t ageMs);
				uint64_t AgeMs() const { return mAgeMs.load(std::memory_order_relaxed); }

			private:
				std::atomic<uint32_t>	mConsecutivePending{ 0 };
				std::atomic<uint64_t>	mAgeMs{ 0 };
				Dia::Observation::Metric::Counter*	mMetricPending = nullptr;	// dia.debugvisualiser.domain_snapshot.reads_pending
				Dia::Observation::Metric::Gauge*	mMetricAge = nullptr;		// dia.debugvisualiser.domain_snapshot.age_ms
			};
		}
	}
}

#endif // DIA_DEBUG
