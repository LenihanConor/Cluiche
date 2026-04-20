#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Time/TimeServer.h>

namespace Cluiche
{
	namespace Sim
	{
		////////////////////////////////////////////////////
		//
		// TimeServerModule: Sim access to its internal clock
		//
		////////////////////////////////////////////////////
		class TimeServerModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			TimeServerModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId = kTypeId, float hz = 30.0f, const Dia::Core::TimeAbsolute& timeNow = Dia::Core::TimeAbsolute::Zero());

			const Dia::Core::TimeServer& GetTimeServer()const;

			void Tick();

		private:
			Dia::Core::TimeServer mTimeServer;
		};
	}
}
