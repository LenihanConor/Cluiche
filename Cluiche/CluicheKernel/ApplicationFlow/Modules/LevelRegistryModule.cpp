#include "CluicheKernel/ApplicationFlow/Modules/LevelRegistryModule.h"

namespace Cluiche
{
	namespace Main
	{
		const Dia::Core::StringCRC LevelRegistryModule::kUniqueId("Main::LevelRegistryModule");

		LevelRegistryModule::LevelRegistryModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kIdle)
		{}
	}
}