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

			if (mCurrentLevel != nullptr)
			{
				DeleteLevel();
			}
		}

		void LevelRegistry::SetCurrentLevel(ILevel* level)
		{
			DIA_ASSERT(mCurrentLevel == nullptr, "There is already an active current level cannot activate a new one");
			
			mCurrentLevel = level;
		}

		void LevelRegistry::DeleteLevel()
		{
			DIA_ASSERT(mCurrentLevel != nullptr, "There is no active current level cannot activate a new one");

			if (mCurrentLevel != nullptr)
			{
				DIA_DELETE(mCurrentLevel);

				mCurrentLevel = nullptr;
			}
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