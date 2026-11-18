#include <DiaDeferredStep/DeferredStepModule.h>

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaDeferredStep/SerialExecutor.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>

#include <json/json.h>

namespace Dia::DeferredStep
{
	namespace
	{
		// Used when no JobSystemService is available (Feature 2 AC 17): every step runs synchronously on SimPU.
		class InlineStepLauncher final : public IStepLauncher
		{
		public:
			LaunchToken Launch(IDeferredStep& step, const LaunchInfo& /*info*/) override
			{
				step.Run(mExecutor);
				LaunchToken token;
				token.value = 1;
				return token;
			}
			bool IsComplete(LaunchToken /*token*/) const override { return true; }
			void Wait(LaunchToken /*token*/) override {}
			void Release(LaunchToken /*token*/) override {}

		private:
			SerialExecutor mExecutor;
		};
	}

	const Dia::Core::StringCRC DeferredStepModule::kTypeId("DeferredStepModule");

	DeferredStepModule::DeferredStepModule(const Dia::Core::StringCRC& instanceId)
		: Dia::ApplicationFlow::SimModule(instanceId)
	{
	}

	// Decides "auto or N" (workerCap 0 = auto) and ranges_per_worker. The clamp against the worker count happens in DoStart.
	// Every rejected shape logs one Warning and falls back to the default; nothing here logs above Warning.
	void DeferredStepModule::OnConfigure(const char* configJson)
	{
		mConfig = CapConfig();
		if (configJson == nullptr || configJson[0] == '\0') { return; }

		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(configJson, root) || !root.isObject())
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "DeferredStepModule config is not a JSON object; using defaults");
			return;
		}

		if (root.isMember("worker_cap"))
		{
			const Json::Value& v = root["worker_cap"];
			if (v.isString() && v.asString() == "auto")
			{
				mConfig.workerCap = 0;
			}
			else if (v.isUInt() && v.asUInt() >= 1)
			{
				mConfig.workerCap = v.asUInt();
			}
			else
			{
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "worker_cap must be \"auto\" or an integer >= 1; falling back to auto");
				mConfig.workerCap = 0;
			}
		}

		if (root.isMember("ranges_per_worker"))
		{
			const Json::Value& v = root["ranges_per_worker"];
			if (v.isUInt() && v.asUInt() >= 1)
			{
				mConfig.rangesPerWorker = v.asUInt();
				if (mConfig.rangesPerWorker > kMaxRangesPerWorker)
				{
					DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "ranges_per_worker above %u; clamped to %u",
						static_cast<unsigned>(kMaxRangesPerWorker), static_cast<unsigned>(kMaxRangesPerWorker));
					mConfig.rangesPerWorker = kMaxRangesPerWorker;
				}
			}
			else if (v.isInt() && v.asInt() < 1)
			{
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "ranges_per_worker below 1; clamped to 1");
				mConfig.rangesPerWorker = 1;
			}
			else
			{
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "ranges_per_worker must be an integer >= 1; using default 4");
				mConfig.rangesPerWorker = CapConfig().rangesPerWorker;
			}
		}
	}

	void DeferredStepModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
	{
		mJobSystemReader.Connect(app);
	}

	DeferredStepScheduler& DeferredStepModule::GetScheduler()
	{
		DIA_ASSERT(mScheduler.Get() != nullptr, "DeferredStepModule::GetScheduler called outside DoStart..DoStop");
		return *mScheduler.Get();
	}

	Dia::ApplicationFlow::StartResult DeferredStepModule::DoStart()
	{
		// Restart safety: tear down any previous instance in reverse construction order.
		mSaveParticipant.Reset();   // destructor detaches if still attached
		mSaveAttached = false;
		mSaveResolved = false;
		mScheduler.Reset();
		mLauncher.Reset();
		mJobLauncher = nullptr;

		const CapConfig& config = mConfig;

		if (mJobSystemReader.IsConnected() && mJobSystemReader.IsAvailable())
		{
			Dia::Threading::JobSystem& jobs = mJobSystemReader.Get();
			mJobLauncher = new JobStepLauncher(jobs, config);
			mLauncher.Reset(mJobLauncher);
			DIA_LOG_INFO(DeferredStepObservation::kLogChannel, "DeferredStep started: cap=%u, rangesPerWorker=%u, workers=%u",
				static_cast<unsigned>(static_cast<JobStepLauncher*>(mLauncher.Get())->GetCapGate().GetWorkerCap()),
				static_cast<unsigned>(config.rangesPerWorker),
				static_cast<unsigned>(jobs.GetWorkerCount()));
		}
		else
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "JobSystemService not available; deferred steps will run inline on SimPU");
			mLauncher.Reset(new InlineStepLauncher());
		}

		mScheduler.Reset(new DeferredStepScheduler(*mLauncher.Get()));
		mSaveParticipant.Reset(new DeferredStepSaveParticipant(*mScheduler.Get()));   // Attach happens in a later task

		mLastSeenStalls = 0;
		mMetrics.Register();
		Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	void DeferredStepModule::SetKeySource(IStepKeySource& source)
	{
		mKeySource = &source;
	}

	void DeferredStepModule::SetSaveServices(Dia::SaveGame::SaveRegistry& registry, Dia::SaveGame::SaveManager& manager)
	{
		mSaveRegistry = &registry;
		mSaveManager = &manager;
	}

	void DeferredStepModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
	{
		if (mScheduler.Get() == nullptr) { return; }

		DIA_TRACE_ZONE("DeferredStep.Update", Dia::Observation::Trace::Category::kNone);
		DIA_PROFILE_SCOPE("DeferredStep.Update", Dia::Observation::Profile::Category::kNone);

		// Lazy attach on the first update so this participant registers after every other module's.
		if (!mSaveResolved)
		{
			mSaveResolved = true;
			if (mSaveRegistry != nullptr && mSaveManager != nullptr && mSaveParticipant.Get() != nullptr)
			{
				mSaveParticipant->Attach(*mSaveRegistry, *mSaveManager);
				mSaveAttached = true;
			}
			else
			{
				DIA_LOG_INFO(DeferredStepObservation::kLogChannel, "No save services injected; DeferredStep does not participate in save/load");
			}
		}

		if (mKeySource == nullptr)
		{
			if (!mWarnedNoKeySource)
			{
				mWarnedNoKeySource = true;
				DIA_LOG_WARNING(DeferredStepObservation::kLogChannel, "DeferredStepModule has no key source; steps will not be ticked until SetKeySource is called");
			}
			return;
		}

		mScheduler->Tick(mKeySource->GetCurrentKey());

		const SchedulerStats& stats = mScheduler->GetStats();
		if (stats.stalls > mLastSeenStalls)
		{
			mHealth.NoteStall();
		}
		mLastSeenStalls = stats.stalls;
		const bool hasCap = mJobLauncher != nullptr;
		mMetrics.Update(stats, hasCap ? mJobLauncher->GetCapGate().GetStats() : CapStats(), hasCap);
	}

	Dia::ApplicationFlow::StopResult DeferredStepModule::DoStop()
	{
		if (mScheduler.Get() != nullptr)
		{
			mScheduler->DrainAndDiscardAll();
			DIA_ASSERT(mScheduler->GetChannelCount() == 0, "DeferredStepModule stopping with channels still registered; consumers must unregister in their own DoStop");

			const SchedulerStats& stats = mScheduler->GetStats();
			DIA_LOG_INFO(DeferredStepObservation::kLogChannel, "DeferredStep stopped: kicks=%llu, applies=%llu, stalls=%llu",
				static_cast<unsigned long long>(stats.kicks),
				static_cast<unsigned long long>(stats.applies),
				static_cast<unsigned long long>(stats.stalls));
		}

		if (mSaveAttached && mSaveParticipant.Get() != nullptr)
		{
			mSaveParticipant->Detach();
		}
		mSaveAttached = false;
		mSaveResolved = false;

		Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
		mMetrics.Unregister();
		mSaveParticipant.Reset();
		mScheduler.Reset();
		mLauncher.Reset();
		mJobLauncher = nullptr;
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

namespace { using DeferredStepModule_ = Dia::DeferredStep::DeferredStepModule; }
DIA_MODULE(DeferredStepModule_);
DIA_DESCRIBE(DeferredStepModule_::kTypeId, "SimPU host for deferred world-step jobs: schedules steps by key, runs them on the JobSystem, applies results at a later key.");
