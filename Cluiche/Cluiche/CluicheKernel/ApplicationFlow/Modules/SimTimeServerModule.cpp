#include "ApplicationFlow/Modules/SimTimeServerModule.h"

namespace Cluiche
{
	namespace Sim
	{
		const Dia::Core::StringCRC TimeServerModule::kUniqueId("Sim::TimeServerModule");

		TimeServerModule::TimeServerModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, float hz, const Dia::Core::TimeAbsolute& timeNow)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kIdle) // idle as we manually update
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