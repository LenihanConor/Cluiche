#pragma once

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobSystem.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/DeferredStepSaveParticipant.h>
#include <DiaDeferredStep/IStepLauncher.h>
#include <DiaDeferredStep/IStepKeySource.h>
#include <DiaDeferredStep/DeferredStepHealth.h>
#include <DiaDeferredStep/DeferredStepMetrics.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaDeferredStep/IStepSummarySink.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaObservation/Profile/ChannelNameAnnounce.h>
#include <chrono>

namespace Dia::SaveGame
{
	class SaveRegistry;
	class SaveManager;
}

namespace Dia::Observation::Profile
{
	class JobTimingTable;
}

namespace Dia::DeferredStep
{
	// SimPU module that hosts the deferred-step scheduler. Consumes the JobSystem
	// published on "JobSystemService" (MainPU). Consumers reach it via ModuleRef<DeferredStepModule>.
	class DeferredStepModule : public Dia::ApplicationFlow::SimModule, private IStepSummarySink
	{
	public:
		static const Dia::Core::StringCRC kTypeId;
		static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
		static constexpr const char* kDescription = "SimPU host for deferred world-step jobs: schedules steps by key, runs them on the JobSystem, applies results at a later key";

		explicit DeferredStepModule(const Dia::Core::StringCRC& instanceId);

		// Re-expose the inherited no-arg overload so ModuleRef<T>::Get() resolves.
		using Dia::ApplicationFlow::Module::GetState;

			// Valid from a successful DoStart until DoStop. Consumers register channels in their own DoStart.
			DeferredStepScheduler& GetScheduler();

			// Non-owning: the caller keeps the source alive from module start to stop. May be called any time
			// before the first DoUpdate (a later call is still accepted; the module warns once until it arrives).
			// DoUpdate reads the source once per sim step and calls scheduler.Tick(source.GetCurrentKey()).
			void SetKeySource(IStepKeySource& source);

			// Non-owning; call before Application::Start. The save participant attaches lazily on the first
			// DoUpdate (so it registers after every other module's participants and deserializes last).
			// Never calling this is fine: the module logs one Info and has no save participation.
			void SetSaveServices(Dia::SaveGame::SaveRegistry& registry, Dia::SaveGame::SaveManager& manager);

			// Non-owning, optional (null = none, the default). Step summaries are forwarded to it when set; the ring
			// below is filled either way. The caller keeps the table alive while set.
			void SetJobTimingTable(Dia::Observation::Profile::JobTimingTable* table) { mJobTimingTable = table; }

			// Optional "$perfnames" sink (non-owning; OnConnectStreams defaults it to the Application's store, null = none).
			// Registered channel names are announced (crc + name, truncated to 63 chars) from DoUpdate whenever the set of
			// channels changed since the last announce, and again every interval, because EventStream readers get no replay
			// and Main's reader may attach after SimPU started. The default interval is 2000 ms (wall clock).
			void SetChannelNameSink(Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::ChannelNameAnnounce>* sink) { mNameSink = sink; }
			void SetChannelNameResendIntervalMs(uint32_t ms) { mNameResendMs = ms; }

			// Last kSummaryRingCapacity step summaries, oldest first. The ring is cleared on every DoStart, so a restart
			// starts empty (it is not cleared by DoStop, so it can be inspected after stop).
			static constexpr uint32_t kSummaryRingCapacity = 16;
			uint32_t GetSummaryCount() const { return mSummaryCount; }
			bool GetSummary(uint32_t indexFromOldest, Dia::Observation::Profile::StepSummaryRecord& out) const;

	protected:
		void OnConfigure(const char* configJson) override;
		void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
		Dia::ApplicationFlow::StartResult DoStart() override;
		void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
		Dia::ApplicationFlow::StopResult DoStop() override;

		// Parsed by OnConfigure: workerCap 0 = auto; resolved against the worker count in DoStart.
		CapConfig mConfig;

		Dia::ApplicationFlow::ServiceStreamReader<Dia::Threading::JobSystem> mJobSystemReader{ this, "JobSystemService" };

			// Built in DoStart (the job system is not available at construction). Declaration order is
			// construction order; members destroy in reverse, so the participant goes first, then the
			// scheduler, then the launcher (which owns the CapGate).
			Dia::Core::UniquePtr<IStepLauncher>                  mLauncher;
			Dia::Core::UniquePtr<DeferredStepScheduler>          mScheduler;
			Dia::Core::UniquePtr<DeferredStepSaveParticipant>    mSaveParticipant;   // constructed only; attach is a later task

			JobStepLauncher*            mJobLauncher = nullptr;   // non-owning view of mLauncher when it is the job launcher (gate access)
			DeferredStepMetrics         mMetrics;
			DeferredStepHealth          mHealth;                   // registered with HealthRegistry between DoStart and DoStop
			uint64_t                    mLastSeenStalls = 0;

			IStepKeySource* mKeySource = nullptr;     // non-owning
			bool            mWarnedNoKeySource = false;

			Dia::SaveGame::SaveRegistry* mSaveRegistry = nullptr;   // non-owning, injected
			Dia::SaveGame::SaveManager*  mSaveManager  = nullptr;   // non-owning, injected
			bool            mSaveAttached = false;
			bool            mSaveResolved = false;   // first-DoUpdate attach decision made (attached or logged no-services)

			// Fixed ring: no allocation on the sink path. mSummaryHead is the oldest slot once the ring is full.
			Dia::Observation::Profile::StepSummaryRecord mSummaryRing[kSummaryRingCapacity];
			uint32_t mSummaryHead = 0;
			uint32_t mSummaryCount = 0;
			Dia::Observation::Profile::JobTimingTable* mJobTimingTable = nullptr;   // non-owning, optional

			Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::ChannelNameAnnounce>* mNameSink = nullptr;   // non-owning, optional
			uint32_t mNameResendMs = 2000;
			uint32_t mAnnouncedCount = 0;                              // channels covered by the last announce
			uint32_t mAnnouncedCrc[DeferredStepScheduler::kMaxChannels] = {};
			bool     mHasAnnounced = false;
			std::chrono::steady_clock::time_point mLastAnnounce;

		private:
			void AnnounceChannelNames();
			void TryAttachJobLauncher();   // late JobSystemService: swap the inline launcher for the job launcher when idle
			void OnStepSummary(const Dia::Observation::Profile::StepSummaryRecord& record) override;
			void ForwardToTable(const Dia::Observation::Profile::StepSummaryRecord& record);
	};
}
