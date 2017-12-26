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
			static const Dia::Core::StringCRC kUniqueId;

			TimeServerModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, float hz, const Dia::Core::TimeAbsolute& timeNow);

			const Dia::Core::TimeServer& GetTimeServer()const;

			void Tick();

		private:
			Dia::Core::TimeServer mTimeServer;
		};
	}
}
