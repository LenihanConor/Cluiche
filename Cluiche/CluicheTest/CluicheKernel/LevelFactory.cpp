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
				DeleteLevel(mCurrentLevel->GetUniqueId());
			}
		}

		void LevelFactory::SetCurrentLevel(ILevel* level)
		{
			DIA_ASSERT(mCurrentLevel == nullptr, "There is already an active current level cannot activate a new one");
			
			mCurrentLevel = level;
		}

		void LevelFactory::DeleteLevel(const Dia::Core::StringCRC& uniqueId)
		{
			ILevel* level = FindLevel(uniqueId);

			DIA_ASSERT(level, "Could not find level: %s", uniqueId.AsChar());

			if (level != nullptr)
			{
				DIA_DELETE(level);

				mCurrentLevel = nullptr;
			}
		}

		ILevel* LevelFactory::FindLevel(const Dia::Core::StringCRC& uniqueId)
		{
		//	for (unsigned int i = 0; i < mLevelArray.Size(); i++)
		//	{
		//		if (mLevelArray[i]. == )
		//	}

			return nullptr;
		}

		const ILevel* LevelFactory::LevelFactory::FindLevel(const Dia::Core::StringCRC& uniqueId)const
		{

			return nullptr;
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