#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Time/TimeServer.h>

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// SimTimeServerModule: Sim access to its internal clock
	//
	////////////////////////////////////////////////////
	class SimTimeServerModule : public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		SimTimeServerModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, float hz, const Dia::Core::TimeAbsolute& timeNow);

		const Dia::Core::TimeServer& GetTimeServer()const;

		void Tick();

	private:
		Dia::Core::TimeServer mTimeServer;
	};
}
