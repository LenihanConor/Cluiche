#pragma once

#include <DiaApplication/ApplicationModule.h>

#include "Cluiche/Source/LevelRegistry.h"

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

		LevelRegistry& GetLevelRegistry() { return mLevelRegistery; }
		const LevelRegistry& GetLevelRegistry()const { return mLevelRegistery; }

	private:
		LevelRegistry mLevelRegistery;
	};
}
