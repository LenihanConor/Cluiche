#include "CluicheKernel/LevelFactory.h"

namespace Cluiche
{
	namespace Kernel
	{
		LevelFactory::LevelFactory()
			: mCurrentLevel(nullptr)
		{}

		LevelFactory::~LevelFactory()
		{
			DIA_ASSERT(mCurrentLevel == nullptr, "Leaking memory");

			if (mCurrentLevel != nullptr)
			{
				DeleteLevel();
			}
		}

		void LevelFactory::SetCurrentLevel(ILevel* level)
		{
			DIA_ASSERT(mCurrentLevel == nullptr, "There is already an active current level cannot activate a new one");
			
			mCurrentLevel = level;
		}

		void LevelFactory::DeleteLevel()
		{
			DIA_ASSERT(mCurrentLevel != nullptr, "There is no active current level cannot activate a new one");

			if (mCurrentLevel != nullptr)
			{
				DIA_DELETE(mCurrentLevel);

				mCurrentLevel = nullptr;
			}
		}

		ILevel* LevelFactory::GetCurrentLevel()
		{
			return mCurrentLevel;
		}

		const ILevel* LevelFactory::GetCurrentLevel()const
		{
			return mCurrentLevel;
		}
	}
}