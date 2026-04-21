#include "GameConnectionModule.h"

#include <DiaCore/Core/Log.h>

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
			Dia::Core::Log::OutputVaradicLine("GameConnectionModule: DoStart - initializing manager");
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
			Dia::Core::Log::OutputVaradicLine("GameConnectionModule: DoStop");
			mController.Shutdown();
			mManager.Shutdown();
		}
	}
}
