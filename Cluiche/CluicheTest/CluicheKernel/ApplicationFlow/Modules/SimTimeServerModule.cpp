#include "ApplicationFlow/Modules/SimTimeServerModule.h"

namespace Cluiche
{
	namespace Sim
	{
		const Dia::Core::StringCRC TimeServerModule::kTypeId("Sim::TimeServerModule");

		TimeServerModule::TimeServerModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId, float hz, const Dia::Core::TimeAbsolute& timeNow)
			: Dia::Application::Module(associatedProcessingUnit, instanceId, Dia::Application::Module::RunningEnum::kIdle)
			, mTimeServer(hz, timeNow)
		{}

		const Dia::Core::TimeServer& TimeServerModule::GetTimeServer()const
		{
			return mTimeServer;
		}

		void TimeServerModule::Tick()
		{
			mTimeServer.Tick();
		}
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
#include <DiaCore/Time/TimeAbsolute.h>
namespace { using _TimeServerModule = Cluiche::Sim::TimeServerModule; }
DIA_REGISTER_MODULE(_TimeServerModule) {
	float hz = config.get("hz", 30.0f).asFloat();
	return new Cluiche::Sim::TimeServerModule(pu, instanceId, hz, Dia::Core::TimeAbsolute::Zero());
}