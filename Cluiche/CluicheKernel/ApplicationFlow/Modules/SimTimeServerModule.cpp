#include "ApplicationFlow/Modules/SimTimeServerModule.h"

namespace Cluiche
{
	const Dia::Core::StringCRC SimTimeServerModule::kUniqueId("SimTimeServerModule");

	SimTimeServerModule::SimTimeServerModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, float hz, const Dia::Core::TimeAbsolute& timeNow)
		: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kIdle) // idle as we manually update
		, mTimeServer(hz, timeNow)
	{}

	const Dia::Core::TimeServer& SimTimeServerModule::GetTimeServer()const
	{
		return mTimeServer;
	}

	void SimTimeServerModule::Tick()
	{
		mTimeServer.Tick();
	}
}