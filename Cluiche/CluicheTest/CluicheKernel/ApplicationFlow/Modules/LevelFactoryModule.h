#pragma once

#include <DiaApplication/ApplicationModule.h>

#include "CluicheKernel/LevelFactory.h"

namespace Cluiche
{
	namespace Main
	{
		////////////////////////////////////////////////////
		//
		// LevelRegisterModule: Central module for level to register themselves with
		//
		////////////////////////////////////////////////////
		class LevelFactoryModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			LevelFactoryModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId = kTypeId);

			Kernel::LevelFactory& GetLevelFactory() { return mLevelRegistery; }
			const Kernel::LevelFactory& GetLevelFactory()const { return mLevelRegistery; }

		private:
			Kernel::LevelFactory mLevelRegistery;
		};
	}
}
