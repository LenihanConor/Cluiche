#include <DiaDeferredStep/DeferredStepModule.h>

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Dia::DeferredStep
{
	const Dia::Core::StringCRC DeferredStepModule::kTypeId("DeferredStepModule");

	DeferredStepModule::DeferredStepModule(const Dia::Core::StringCRC& instanceId)
		: Dia::ApplicationFlow::SimModule(instanceId)
	{
	}

	void DeferredStepModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
	{
		mJobSystemReader.Connect(app);
	}

	Dia::ApplicationFlow::StartResult DeferredStepModule::DoStart()
	{
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	void DeferredStepModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
	{
	}

	Dia::ApplicationFlow::StopResult DeferredStepModule::DoStop()
	{
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

namespace { using DeferredStepModule_ = Dia::DeferredStep::DeferredStepModule; }
DIA_MODULE(DeferredStepModule_);
DIA_DESCRIBE(DeferredStepModule_::kTypeId, "SimPU host for deferred world-step jobs: schedules steps by key, runs them on the JobSystem, applies results at a later key.");
