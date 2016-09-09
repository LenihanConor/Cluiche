#include "ApplicationFlow/Modules/LevelRegistryModule.h"

namespace Cluiche
{
	const Dia::Core::StringCRC LevelRegistryModule::kUniqueId("LevelRegistryModule");

	LevelRegistryModule::LevelRegistryModule(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kIdle)
	{}
}