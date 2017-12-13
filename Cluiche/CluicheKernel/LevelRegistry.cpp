#include "CluicheKernel/LevelRegistry.h"

namespace Cluiche
{
	namespace Kernel
	{
		LevelRegistry::LevelRegistry()
			: mCurrentLevel(nullptr)
		{}

		LevelRegistry::~LevelRegistry()
		{
			DIA_ASSERT(mCurrentLevel == nullptr, "Leaking memory");
		}

		void LevelRegistry::SetCurrentLevel(ILevel* level)
		{
			DIA_ASSERT(mCurrentLevel == nullptr, "There is already an active current level cannot activate a new one");
			
			mCurrentLevel = level;
		}

		ILevel* LevelRegistry::GetCurrentLevel()
		{
			return mCurrentLevel;
		}

		const ILevel* LevelRegistry::GetCurrentLevel()const
		{
			return mCurrentLevel;
		}
	}
}