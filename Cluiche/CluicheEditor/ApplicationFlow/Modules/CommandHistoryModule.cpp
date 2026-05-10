#include "CommandHistoryModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC CommandHistoryModule::kTypeId("CommandHistoryModule");

		CommandHistoryModule::CommandHistoryModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
		{
		}

		Dia::ApplicationFlow::StartResult CommandHistoryModule::DoStart()
		{
			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void CommandHistoryModule::DoUpdate(float /*deltaTime*/)
		{
		}

		Dia::ApplicationFlow::StopResult CommandHistoryModule::DoStop()
		{
			mHistory.Clear();
			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using CommandHistoryModule_ = Cluiche::Editor::CommandHistoryModule; }
DIA_MODULE(CommandHistoryModule_);
