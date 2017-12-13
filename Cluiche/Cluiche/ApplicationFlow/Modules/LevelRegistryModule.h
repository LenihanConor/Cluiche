#pragma once

#include <DiaApplication/ApplicationModule.h>

#include "CluicheKernel/LevelRegistry.h"

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// LevelRegisterModule: Central module for level to register themselves with
	//
	////////////////////////////////////////////////////
	class LevelRegistryModule : public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		LevelRegistryModule(Dia::Application::ProcessingUnit* associatedProcessingUnit);

		Kernel::LevelRegistry& GetLevelRegistry() { return mLevelRegistery; }
		const Kernel::LevelRegistry& GetLevelRegistry()const { return mLevelRegistery; }

	private:
		Kernel::LevelRegistry mLevelRegistery;
	};
}
