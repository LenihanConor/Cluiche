#include "CommandHistoryModule.h"

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC CommandHistoryModule::kTypeId("CommandHistoryModule");

		CommandHistoryModule::CommandHistoryModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kIdle)
		{
		}

		void CommandHistoryModule::DoStop()
		{
			mHistory.Clear();
		}
	}
}
