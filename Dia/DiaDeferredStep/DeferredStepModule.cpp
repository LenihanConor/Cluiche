#include <DiaDeferredStep/DeferredStepModule.h>

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/CapConfig.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/JobStepLauncher.h>
#include <DiaDeferredStep/SerialExecutor.h>
#include <DiaObservation/Log/DiaLog.h>

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
		mSaveParticipant.Reset();
		mScheduler.Reset();
		mLauncher.Reset();

		// TODO(task 3): resolve the cap from configuration. For now "auto" (workerCap 0) and 4 ranges per worker.
		const CapConfig config;

		if (mJobSystemReader.IsConnected() && mJobSystemReader.IsAvailable())
		{
			Dia::Threading::JobSystem& jobs = mJobSystemReader.Get();
			mLauncher.Reset(new JobStepLauncher(jobs, config));
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

		// SEAM(metrics/health task): register metrics and the IHealthReporter here; unregister in DoStop.
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	void DeferredStepModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
	{
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

		// SEAM(save attach task): Detach the save participant here if attached.
		// SEAM(metrics/health task): unregister metrics and the health reporter here.
		mSaveParticipant.Reset();
		mScheduler.Reset();
		mLauncher.Reset();
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

namespace { using DeferredStepModule_ = Dia::DeferredStep::DeferredStepModule; }
DIA_MODULE(DeferredStepModule_);
DIA_DESCRIBE(DeferredStepModule_::kTypeId, "SimPU host for deferred world-step jobs: schedules steps by key, runs them on the JobSystem, applies results at a later key.");
