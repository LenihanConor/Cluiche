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

namespace Dia::SaveGame
{
	class SaveRegistry;
	class SaveManager;
}

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

			// Non-owning: the caller keeps the source alive from module start to stop. May be called any time
			// before the first DoUpdate (a later call is still accepted; the module warns once until it arrives).
			// DoUpdate reads the source once per sim step and calls scheduler.Tick(source.GetCurrentKey()).
			void SetKeySource(IStepKeySource& source);

			// Non-owning; call before Application::Start. The save participant attaches lazily on the first
			// DoUpdate (so it registers after every other module's participants and deserializes last).
			// Never calling this is fine: the module logs one Info and has no save participation.
			void SetSaveServices(Dia::SaveGame::SaveRegistry& registry, Dia::SaveGame::SaveManager& manager);

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

			IStepKeySource* mKeySource = nullptr;     // non-owning
			bool            mWarnedNoKeySource = false;

			Dia::SaveGame::SaveRegistry* mSaveRegistry = nullptr;   // non-owning, injected
			Dia::SaveGame::SaveManager*  mSaveManager  = nullptr;   // non-owning, injected
			bool            mSaveAttached = false;
			bool            mSaveResolved = false;   // first-DoUpdate attach decision made (attached or logged no-services)
	};
}
