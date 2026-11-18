#pragma once

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobSystem.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/DeferredStepSaveParticipant.h>
#include <DiaDeferredStep/IStepLauncher.h>

namespace Dia::DeferredStep
{
	// SimPU module that hosts the deferred-step scheduler. Consumes the JobSystem
	// published on "JobSystemService" (MainPU). Consumers reach it via ModuleRef<DeferredStepModule>.
	class DeferredStepModule : public Dia::ApplicationFlow::SimModule
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

	protected:
		void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
		Dia::ApplicationFlow::StartResult DoStart() override;
		void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
		Dia::ApplicationFlow::StopResult DoStop() override;

		Dia::ApplicationFlow::ServiceStreamReader<Dia::Threading::JobSystem> mJobSystemReader{ this, "JobSystemService" };

			// Built in DoStart (the job system is not available at construction). Declaration order is
			// construction order; members destroy in reverse, so the participant goes first, then the
			// scheduler, then the launcher (which owns the CapGate).
			Dia::Core::UniquePtr<IStepLauncher>                  mLauncher;
			Dia::Core::UniquePtr<DeferredStepScheduler>          mScheduler;
			Dia::Core::UniquePtr<DeferredStepSaveParticipant>    mSaveParticipant;   // constructed only; attach is a later task
	};
}
