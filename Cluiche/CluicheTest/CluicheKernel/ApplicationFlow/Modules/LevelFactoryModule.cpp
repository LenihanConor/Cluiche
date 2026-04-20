#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"

namespace Cluiche
{
	namespace Main
	{
		const Dia::Core::StringCRC LevelFactoryModule::kTypeId("Main::LevelRegistryModule");

		LevelFactoryModule::LevelFactoryModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
			: Dia::Application::Module(associatedProcessingUnit, instanceId, Dia::Application::Module::RunningEnum::kIdle)
		{}
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _LevelFactoryModule = Cluiche::Main::LevelFactoryModule; }
DIA_REGISTER_MODULE(_LevelFactoryModule) {
	return new Cluiche::Main::LevelFactoryModule(pu, instanceId);
}