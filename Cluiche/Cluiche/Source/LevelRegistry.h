#pragma once

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche
{
	/// This is a registry of all level, Levels are all code that can be flowed into via the phases
	class LevelRegistry
	{
	public:
		// This is the hashed data about the game
		struct Data
		{
			Data()
			{}

			Data(const Dia::Core::StringCRC& entryPhase)
				: mEntryPhase(entryPhase)
			{}

			Dia::Core::StringCRC mEntryPhase;
		};

		LevelRegistry();
		~LevelRegistry();
	
		void LevelRegistry::Register(const Dia::Core::StringCRC& name, const LevelRegistry::Data& data);
		const Dia::Core::StringCRC& FetchEntryPhase(Dia::Core::StringCRC& levelName)const;

	private:
		typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Data, Dia::Core::StringCRCHashFunctor> LevelTable;
		
		// Essentially this is a hash of name to struct
		LevelTable mTable;
	};
}
