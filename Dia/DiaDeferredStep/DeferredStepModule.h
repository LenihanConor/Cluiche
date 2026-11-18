#pragma once

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaThreading/JobSystem.h>

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

	protected:
		void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
		Dia::ApplicationFlow::StartResult DoStart() override;
		void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
		Dia::ApplicationFlow::StopResult DoStop() override;

		Dia::ApplicationFlow::ServiceStreamReader<Dia::Threading::JobSystem> mJobSystemReader{ this, "JobSystemService" };
	};
}
