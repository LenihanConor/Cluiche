#include "Source/LevelRegistry.h"

namespace Cluiche
{
	LevelRegistry::LevelRegistry()
		: mTable(8, 16)
	{}

	LevelRegistry::~LevelRegistry()
	{
		mTable.RemoveAll();
	}

	void LevelRegistry::Register(const Dia::Core::StringCRC& name, const LevelRegistry::Data& data)
	{
		mTable.Add(name, data);
	}

	const Dia::Core::StringCRC& LevelRegistry::FetchEntryPhase(Dia::Core::StringCRC& levelName)const
	{
		const Data* pData = mTable.TryGetItemConst(levelName);
		
		if (pData == nullptr)
		{
			return Dia::Core::StringCRC::kZero;
		}

		return pData->mEntryPhase;
	}
}