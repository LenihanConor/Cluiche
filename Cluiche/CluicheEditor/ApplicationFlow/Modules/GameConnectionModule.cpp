#include "GameConnectionModule.h"
#include "EditorModelModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC GameConnectionModule::kTypeId("GameConnectionModule");

		GameConnectionModule::GameConnectionModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
			, mModelRef(this, EditorModelModule::kTypeId)
		{
		}

		Dia::ApplicationFlow::StartResult GameConnectionModule::DoStart()
		{
			mConnectionManager.Initialize();
			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void GameConnectionModule::DoUpdate(float deltaTime)
		{
			mConnectionManager.Update(deltaTime);
		}

		Dia::ApplicationFlow::StopResult GameConnectionModule::DoStop()
		{
			mConnectionManager.Shutdown();
			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using GameConnectionModule_ = Cluiche::Editor::GameConnectionModule; }
DIA_MODULE(GameConnectionModule_);
