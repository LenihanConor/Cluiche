#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"

namespace Cluiche
{
	namespace Main
	{
		const Dia::Core::StringCRC LevelFactoryModule::kUniqueId("Main::LevelRegistryModule");

		LevelFactoryModule::LevelFactoryModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kIdle)
		{}
	}
}