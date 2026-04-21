#include "GameConnectionModule.h"

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC GameConnectionModule::kTypeId("GameConnectionModule");

		GameConnectionModule::GameConnectionModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kUpdate)
		{
		}

		Dia::Application::StateObject::OpertionResponse GameConnectionModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			mManager.Initialize();
			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void GameConnectionModule::DoUpdate()
		{
			mManager.Update(0.016f);
			mController.Update(0.016f);
		}

		void GameConnectionModule::DoStop()
		{
			mController.Shutdown();
			mManager.Shutdown();
		}
	}
}
